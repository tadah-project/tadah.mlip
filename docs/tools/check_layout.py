#!/usr/bin/env python3
"""Check built HTML pages for horizontal overflow in a headless browser.

    check_layout.py <html_dir> [--pages GLOB ...] [--widths W,W,...]
                    [--browser PATH]

Every page is loaded from disk in a headless Chromium-family browser
(Chromium, Chromium Browser or Google Chrome) at each window width (by
default 1280, 1024, 900, 768 and 390 px), and a page fails at a width when

  * the document is wider than the window (the scrollWidth of the document
    exceeds the window width); the problem names the deepest elements that
    reach past the right edge of the window;
  * a code block (``pre``, ``.highlight``), a table wrapper
    (``.wy-table-responsive``), a field list (``dl.field-list``), display
    maths (``div.math``) or an image (``img``) is wider than its box
    (scrollWidth > clientWidth + 1 px: it scrolls, or its content sticks
    out) or reaches past the text column (the page's ``role="main"``
    element).

Elements of the navigation sidebar are not measured. Each problem is one
line, ``<page>:<element>:<width>px: <what>``, where <element> is the kind,
its number among the page's elements of that kind and the id of the
section it is in (``pre[3]#choosing-the-budget``).

--pages takes glob patterns relative to <html_dir> (``nfp/**/*.html``); by
default every ``.html`` file is checked except those under directories
whose name starts with ``_`` (``_static``, ``_sources``, ...).

The browser has no network: names of other hosts do not resolve, so scripts
a page loads from elsewhere (MathJax from its CDN) do not run, and display
maths is measured as the TeX source the page holds.

Exit status: 0 when no page has a problem, 1 when one has, 2 on a usage
error (no page matches, no browser, a page that does not load).
"""
import argparse
import fcntl
import json
import os
import select
import shutil
import subprocess
import sys
import tempfile
from collections import namedtuple
from pathlib import Path

WIDTHS = (1280, 1024, 900, 768, 390)
HEIGHT = 900
BROWSERS = ('chromium', 'chromium-browser', 'google-chrome')
TIMEOUT = 60   # seconds for one browser reply or one page load

Problem = namedtuple('Problem', 'page element width detail')
Problem.__str__ = lambda p: f'{p.page}:{p.element}:{p.width}px: {p.detail}'


class LayoutError(Exception):
    """The check could not run (no browser, a page that does not load)."""


# Runs in the page after the load event; resolves to a JSON-able object.
MEASURE = r'''
(async () => {
  if (document.fonts && document.fonts.ready) { await document.fonts.ready; }
  await new Promise(r => requestAnimationFrame(() => requestAnimationFrame(r)));
  const W = window.innerWidth;
  const out = {window: W, problems: []};
  const inSidebar = el => el.closest('.wy-nav-side, .sphinxsidebar') !== null;
  const where = el => {
    const s = el.parentElement && el.parentElement.closest('section[id], div.section[id]');
    return s ? '#' + s.id : '';
  };
  const snippet = (el, kind) => {
    if (kind === 'img') { return (el.getAttribute('src') || '').split('/').pop(); }
    let text = (el.innerText || el.textContent || '');
    if (kind === 'pre' || kind === '.highlight') {
      text = text.split('\n').reduce((a, b) => b.length > a.length ? b : a, '');
    }
    text = text.replace(/\s+/g, ' ').trim();
    return text.length > 60 ? text.slice(0, 57) + '...' : text;
  };
  const doc = Math.max(document.documentElement.scrollWidth,
                       document.body ? document.body.scrollWidth : 0);
  // Inside a box that clips or scrolls its content (a code block that
  // scrolls), an element cannot widen the document.
  const clipped = el => {
    for (let a = el.parentElement; a && a !== document.body; a = a.parentElement) {
      if (getComputedStyle(a).overflowX !== 'visible') { return true; }
    }
    return false;
  };
  // How far right an element or its content (a word that does not
  // break) reaches.
  const reach = el => {
    const r = el.getBoundingClientRect();
    return r.left + Math.max(r.width, el.scrollWidth);
  };
  if (doc > W) {
    const wide = Array.from(document.body.querySelectorAll('*')).filter(el =>
      el.getBoundingClientRect().width > 0 && reach(el) > W + 1 &&
      !inSidebar(el) && !clipped(el));
    const deepest = wide.filter(el => !wide.some(o => o !== el && el.contains(o)));
    const names = deepest.slice(0, 3).map(el => {
      const cls = el.classList.length ? '.' + Array.from(el.classList).join('.') : '';
      return el.tagName.toLowerCase() + cls + where(el) +
             ' (right edge ' + Math.round(reach(el)) + ')';
    });
    out.problems.push({element: 'document',
                       detail: 'scrollWidth ' + doc + ' > window ' + W +
                               (names.length ? '; ' + names.join(', ') : '')});
  }
  const main = document.querySelector('[role="main"]') ||
               document.querySelector('.rst-content') || document.body;
  const column = main.getBoundingClientRect();
  for (const kind of ['pre', '.highlight', '.wy-table-responsive', 'dl.field-list',
                      'div.math', 'img']) {
    let n = 0;
    for (const el of document.querySelectorAll(kind)) {
      if (inSidebar(el)) { continue; }
      n += 1;
      const r = el.getBoundingClientRect();
      if (r.width === 0 && r.height === 0) { continue; }
      const name = kind.replace(/^\./, '') + '[' + n + ']' + where(el);
      const text = snippet(el, kind);
      const tail = text ? ' "' + text + '"' : '';
      if (el.scrollWidth > el.clientWidth + 1) {
        out.problems.push({element: name, detail: 'scrollWidth ' + el.scrollWidth +
                           ' > clientWidth ' + el.clientWidth + tail});
      } else if (r.right > column.right + 1) {
        out.problems.push({element: name, detail: 'right edge ' + Math.round(r.right) +
                           ' > text column ' + Math.round(column.right) + tail});
      }
    }
  }
  return out;
})()
'''


def find_browser(explicit=None):
    """The browser to run: *explicit*, else the first of BROWSERS on PATH."""
    if explicit:
        path = shutil.which(explicit)
        if not path:
            raise LayoutError(f'--browser {explicit}: no such executable')
        return path
    for name in BROWSERS:
        path = shutil.which(name)
        if path:
            return path
    return None


def select_pages(html_dir, globs=None):
    """Pages to check, as paths relative to *html_dir*, sorted."""
    root = Path(html_dir)
    if not root.is_dir():
        raise LayoutError(f'{html_dir} is not a directory')
    found = set()
    for pattern in globs or ['**/*.html']:
        for path in root.glob(pattern):
            rel = path.relative_to(root)
            if not path.is_file() or path.suffix != '.html':
                continue
            if not globs and any(part.startswith('_') for part in rel.parts[:-1]):
                continue
            found.add(rel.as_posix())
    return sorted(found)


class Browser:
    """A headless browser driven over the DevTools protocol on a pipe
    (--remote-debugging-pipe: the browser reads commands on its fd 3 and
    writes replies on its fd 4, each message a JSON text ended by NUL)."""

    def __init__(self, executable):
        self.profile = tempfile.mkdtemp(prefix='tadah_layout_')
        to_r, to_w = os.pipe()
        from_r, from_w = os.pipe()

        def child_fds():
            # Move the browser's ends to fds 3 and 4 (inheritable); the
            # high copies close on exec.
            a = fcntl.fcntl(to_r, fcntl.F_DUPFD_CLOEXEC, 10)
            b = fcntl.fcntl(from_w, fcntl.F_DUPFD_CLOEXEC, 10)
            os.dup2(a, 3)
            os.dup2(b, 4)

        args = [executable, '--headless', '--remote-debugging-pipe',
                '--no-sandbox', '--disable-gpu', '--disable-dev-shm-usage',
                '--no-first-run', '--no-default-browser-check',
                '--disable-extensions', '--disable-background-networking',
                '--disable-component-update', '--disable-sync',
                '--hide-scrollbars', '--mute-audio', '--no-proxy-server',
                # No network: every host name fails to resolve.
                '--host-resolver-rules=MAP * ~NOTFOUND',
                f'--user-data-dir={self.profile}', 'about:blank']
        try:
            self.proc = subprocess.Popen(args, preexec_fn=child_fds,
                                         close_fds=False,
                                         stdin=subprocess.DEVNULL,
                                         stdout=subprocess.DEVNULL,
                                         stderr=subprocess.DEVNULL)
        finally:
            os.close(to_r)
            os.close(from_w)
        self.out = to_w
        self.inp = from_r
        self.buf = b''
        self.next_id = 0
        self.events = []
        try:
            target = self.call('Target.createTarget', {'url': 'about:blank'})
            self.session = self.call('Target.attachToTarget',
                                     {'targetId': target['targetId'],
                                      'flatten': True})['sessionId']
            self.call('Page.enable', session=True)
        except BaseException:
            self.close()
            raise

    def _read(self):
        while b'\0' not in self.buf:
            ready, _, _ = select.select([self.inp], [], [], TIMEOUT)
            if not ready:
                raise LayoutError(f'the browser sent nothing for {TIMEOUT} s')
            chunk = os.read(self.inp, 1 << 16)
            if not chunk:
                raise LayoutError('the browser closed its DevTools pipe '
                                  f'(exit status {self.proc.poll()})')
            self.buf += chunk
        message, self.buf = self.buf.split(b'\0', 1)
        return json.loads(message)

    def call(self, method, params=None, session=False):
        self.next_id += 1
        message = {'id': self.next_id, 'method': method,
                   'params': params or {}}
        if session:
            message['sessionId'] = self.session
        os.write(self.out, json.dumps(message).encode() + b'\0')
        while True:
            reply = self._read()
            if reply.get('id') == self.next_id:
                if 'error' in reply:
                    raise LayoutError(f'{method}: {reply["error"]}')
                return reply['result']
            if 'method' in reply:
                self.events.append(reply)

    def wait_event(self, method):
        while True:
            for i, event in enumerate(self.events):
                if (event['method'] == method
                        and event.get('sessionId') == self.session):
                    del self.events[:i + 1]
                    return event
            self.events.clear()
            reply = self._read()
            if 'method' in reply:
                self.events.append(reply)

    def measure(self, url, width):
        self.call('Emulation.setDeviceMetricsOverride',
                  {'width': width, 'height': HEIGHT, 'deviceScaleFactor': 1,
                   'mobile': False}, session=True)
        self.events.clear()
        nav = self.call('Page.navigate', {'url': url}, session=True)
        if nav.get('errorText'):
            raise LayoutError(f'{url} did not load: {nav["errorText"]}')
        self.wait_event('Page.loadEventFired')
        result = self.call('Runtime.evaluate',
                           {'expression': MEASURE, 'awaitPromise': True,
                            'returnByValue': True}, session=True)
        if 'exceptionDetails' in result:
            raise LayoutError(f'{url}: the measurement failed: '
                              f'{result["exceptionDetails"]}')
        return result['result']['value']

    def close(self):
        try:
            self.call('Browser.close')
        except (LayoutError, OSError):
            pass
        try:
            self.proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            self.proc.kill()
            self.proc.wait()
        for fd in (self.out, self.inp):
            try:
                os.close(fd)
            except OSError:
                pass
        shutil.rmtree(self.profile, ignore_errors=True)

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()


def check(html_dir, pages=None, widths=WIDTHS, browser=None):
    """Problems of *pages* (relative to *html_dir*; all by default) at each
    of *widths*, as a list of Problem."""
    if pages is None:
        pages = select_pages(html_dir)
    executable = find_browser(browser)
    if not executable:
        raise LayoutError('no Chromium-family browser found (tried '
                          + ', '.join(BROWSERS) + ')')
    root = Path(html_dir).resolve()
    problems = []
    with Browser(executable) as b:
        for page in pages:
            url = (root / page).as_uri()
            for width in widths:
                report = b.measure(url, width)
                if report['window'] != width:
                    raise LayoutError(f'{page}: the window is '
                                      f'{report["window"]} px wide, not {width}')
                for p in report['problems']:
                    problems.append(Problem(page, p['element'], width,
                                            p['detail']))
    return problems


def parse_widths(text):
    try:
        widths = tuple(int(w) for w in text.split(','))
    except ValueError:
        raise argparse.ArgumentTypeError(f'not a list of integers: {text}')
    if not widths or any(w < 200 for w in widths):
        raise argparse.ArgumentTypeError(f'widths must be 200 px or more: {text}')
    return widths


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('html_dir', help='HTML output directory of sphinx-build')
    ap.add_argument('--pages', action='append', metavar='GLOB',
                    help='pages to check, relative to html_dir (repeatable)')
    ap.add_argument('--widths', type=parse_widths, default=WIDTHS,
                    help='window widths in px (default: %(default)s)')
    ap.add_argument('--browser', help='browser executable (default: the first '
                    'of ' + ', '.join(BROWSERS) + ' on PATH)')
    args = ap.parse_args(argv)
    try:
        pages = select_pages(args.html_dir, args.pages)
        if not pages:
            raise LayoutError(f'no page of {args.html_dir} matches '
                              f'{args.pages or ["**/*.html"]}')
        problems = check(args.html_dir, pages, args.widths, args.browser)
    except LayoutError as err:
        print(f'check_layout: {err}', file=sys.stderr)
        return 2
    for p in problems:
        print(p)
    widths = ', '.join(str(w) for w in args.widths)
    if problems:
        print(f'check_layout: {len(problems)} problem(s) in {len(pages)} '
              f'page(s) at {widths} px', file=sys.stderr)
        return 1
    print(f'check_layout: {len(pages)} page(s) fit at {widths} px')
    return 0


if __name__ == '__main__':
    sys.exit(main())
