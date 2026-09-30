NNFinder
========

``NNFinder`` builds the neighbour list of a structure: for every atom, every
periodic image of every atom closer than the cutoff :math:`R`.

It works for any cell: orthogonal or triclinic, thicker or thinner than the
cutoff in any direction, including a skewed description of a benign lattice.
The atoms may lie anywhere; they do not have to be wrapped into the cell.

The list (``NNTable``, stored in ``Structure::nn``) is:

- **complete**: atom :math:`i` lists every :math:`(j, \mathbf{s})` with
  :math:`|\mathbf{r}_j + s_0\mathbf{a}_0 + s_1\mathbf{a}_1 + s_2\mathbf{a}_2 -
  \mathbf{r}_i| < R`.

  - :math:`\mathbf{a}_k` are the cell rows and :math:`\mathbf{s}` is an
    integer triple.
  - :math:`j = i` is allowed when :math:`\mathbf{s} \ne 0`.
  - The list may also hold pairs up to :math:`R` plus ``NNTable::guard``.
    That is twice the rounding bound of a computed distance,
    :math:`32\varepsilon (G + 2)(R + 2\max_i |\mathbf{r}_i|)` with
    :math:`G = \sum_k |\mathbf{a}_k||\mathbf{b}^*_k|`: about
    :math:`10^{-13}` relative for atoms near the cell, growing with their
    distance from the origin.
  - A consumer that computes a distance below :math:`R` always finds the
    pair. Consumers filter by their own cutoff.
- **relative to the stored positions**: the positions are never changed. An
  atom outside the cell gets the shift that places its image.
- **symmetric and ordered**:

  - :math:`(i \to j, \mathbf{s})` is listed if and only if
    :math:`(j \to i, -\mathbf{s})` is;
  - no :math:`(j, \mathbf{s})` appears twice;
  - each atom's entries are sorted by :math:`(j, s_0, s_1, s_2)`.
- **self-describing**:

  - ``NNTable::disp`` holds :math:`\mathbf{s}\cdot A`;
  - ``NNTable::cutoff`` is :math:`R`;
  - consumers such as the descriptor calculator refuse a list built with a
    smaller cutoff than they need.

How the list is built:

1. The lattice basis is reduced. This is an integer change of basis, so the
   lattice is the same.
2. The atoms go into a cell list over their wrapped fractional coordinates.
   The list is sized from the cell heights, the spacings of the lattice
   planes.
3. Each pair is tested once, with the distance computed as consumers compute
   it.

Refusals:

- A cell so small for the cutoff that the search would span more than
  :math:`10^4` image cells is refused with ``std::runtime_error``. The count,
  :math:`\prod_k (2N_k + 1)` with :math:`N_k = \lfloor R/h_k \rfloor + 1`
  over the heights of the reduced cell, has no rounding margin, so it does
  not depend on where the atoms are stored: a cube is refused once the
  cutoff reaches ten edges.
- Invalid input is refused with ``std::invalid_argument``:

  - a cutoff that is not a finite positive number;
  - a non-finite cell entry or position;
  - a cell vector shorter than :math:`2^{-200}` Å or longer than
    :math:`2^{200}` Å (about :math:`6\cdot10^{-61}` to
    :math:`1.6\cdot10^{60}` Å), where the cell geometry would under- or
    overflow;
  - a singular or numerically singular cell;
  - an atom more than :math:`2^{31}` cells from the origin;
  - positions whose rounding bound exceeds an image cell (an atom far out
    in a cell near the conditioning limit).

A structure without atoms gets an empty list; its cell is not examined. Two
atoms at the same position are listed as neighbours at distance 0; the
descriptors divide by the distance, so such a structure is not usable.

.. versionchanged:: 1.6.0
   Earlier versions missed periodic images when the cell was thinner than the
   cutoff in some direction, or was triclinic, or held atoms outside the cell.
   For example, a 2-atom bcc cell with :math:`a = 3.41` Å at a 10 Å cutoff
   listed 168 of each atom's 180 neighbours.

.. doxygenclass:: tadah::mlip::NNFinder
    :members:
