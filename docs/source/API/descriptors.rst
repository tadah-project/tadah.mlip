.. _DESCRIPTORS:

Descriptors
===========

Two-Body
--------
List of all two-body descriptors supported by Tadah!

D2_LJ
.....

.. doxygenclass:: D2_LJ
    :members:

D2_BP
.....

.. doxygenclass:: D2_BP
    :members:

D2_Blip
.......

.. doxygenclass:: D2_Blip
    :members:

D2_EAM
......

.. doxygenclass:: D2_EAM
    :members:

D2_Dummy
........

.. doxygenclass:: D2_Dummy
    :members:

D2_Base
.......

.. doxygenclass:: D2_Base
    :members:

D2_mJoin
........

.. doxygenclass:: D2_mJoin
    :members:

Three-Body
----------
As is stands Tadah! does not support three-body type descriptors.
They are simply slow to compute and there exist a number of angularly dependend
many-body descriptors. However if there is enough interest in three-body
descriptors they can be fairly easily added to the library. Just drop us an email...

Many-Body
----------
List of all many-body type descriptors supported by Tadah!
Note that some many-body type descriptors can calculate non-spherical
charge distributionis hence they overcome Embedded Atom Method limitations.

DM_EAD
......

.. doxygenclass:: DM_EAD
    :members:

DM_EAM
......

.. doxygenclass:: DM_EAM
    :members:

DM_Dummy
........

.. doxygenclass:: DM_Dummy
    :members:

DM_Base
.......

.. doxygenclass:: DM_Base
    :members:
