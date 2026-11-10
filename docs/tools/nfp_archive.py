"""Build the example download with stable member order, metadata and bytes."""
from pathlib import Path
import io
import os
import tempfile
import zipfile


def source_files(directory):
    directory = Path(directory)
    if directory.is_symlink():
        raise ValueError(f'archive source is a symlink: {directory}')
    if not directory.is_dir():
        raise ValueError(f'archive source is not a directory: {directory}')
    files = []
    for path in sorted(directory.rglob('*')):
        if path.is_symlink():
            raise ValueError(f'archive source contains a symlink: {path}')
        if path.is_file():
            files.append(path)
        elif not path.is_dir():
            raise ValueError(f'archive source is not a regular file: {path}')
    return files


def build_archive(directory, output):
    """Write a ZIP_STORED archive, replacing *output* only if bytes change.

    The top-level ``files/`` directory preserves the relative input paths.
    Storing members avoids compression-library differences between builders.
    """
    directory, output = Path(directory), Path(output)
    files = source_files(directory)
    if output.resolve().is_relative_to(directory.resolve()):
        raise ValueError(f'archive output is inside its source: {output}')
    buffer = io.BytesIO()
    with zipfile.ZipFile(buffer, 'w', compression=zipfile.ZIP_STORED) as archive:
        for path in files:
            name = 'files/' + path.relative_to(directory).as_posix()
            info = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
            info.create_system = 3
            info.external_attr = 0o100644 << 16
            archive.writestr(info, path.read_bytes())
    data = buffer.getvalue()
    if output.is_file() and output.read_bytes() == data:
        return output
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=output.parent, delete=False) as stream:
        temporary = Path(stream.name)
        try:
            stream.write(data)
            stream.close()
            temporary.chmod(0o644)
            os.replace(temporary, output)
        finally:
            temporary.unlink(missing_ok=True)
    return output


def prepare_download(app):
    chapter = Path(app.srcdir) / 'nfp'
    build_archive(chapter / 'files', chapter / 'nfp-files.zip')


def note_sources(app, docname, source):
    if docname.startswith('nfp/'):
        for path in source_files(Path(app.srcdir) / 'nfp' / 'files'):
            app.env.note_dependency(str(path))


def setup(app):
    app.connect('builder-inited', prepare_download)
    app.connect('source-read', note_sources)
    return {'parallel_read_safe': True, 'parallel_write_safe': True}
