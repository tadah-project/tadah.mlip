.. _DESCRIPTORS:

Descriptors
===========

Two-Body
--------
List of all two-body descriptors supported by Tadah!

D2_LJ
.....

.. doxygenclass:: tadah::models::D2_LJ
    :members:

D2_BP
.....

.. doxygenclass:: tadah::models::D2_BP
    :members:

D2_Blip
.......

.. doxygenclass:: tadah::models::D2_Blip
    :members:

D2_EAM
......

.. doxygenclass:: tadah::models::D2_EAM
    :members:

D2_Dummy
........

.. doxygenclass:: tadah::models::D2_Dummy
    :members:

D2_Base
.......

.. doxygenclass:: tadah::models::D2_Base
    :members:

D2_mJoin
........

.. doxygenclass:: tadah::models::D2_mJoin
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

.. doxygenclass:: tadah::models::DM_EAD
    :members:

DM_EAM
......

.. doxygenclass:: tadah::models::DM_EAM
    :members:

DM_Dummy
........

.. doxygenclass:: tadah::models::DM_Dummy
    :members:

DM_Base
.......

.. doxygenclass:: tadah::models::DM_Base
    :members:

DM_mJoin
........

.. doxygenclass:: tadah::models::DM_mJoin
    :members:
