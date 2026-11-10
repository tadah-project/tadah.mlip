.. _glossary:

Glossary and Abbreviations
==========================

.. glossary::

   Command Line Interface
      A way to interact with the program by typing commands into a terminal or console.

   Nested fitting
      The procedure that chooses the :term:`hyperparameters <hyperparameter>`
      of a potential by a search in which each :term:`evaluation` fits the
      :term:`coefficients <coefficient>` and scores the potential with a
      loss; ``tadah hpo`` runs it (:ref:`nested_fitting`).

   Hyperparameter
      A setting of a potential that a fit does not determine, such as a
      cutoff or the width of a basis function.

   Coefficient
      A number of a potential that the fit determines from the training
      data by regression, unless :ref:`FIXINDEX` pins it to its
      :ref:`FIXWEIGHT` value; the key :ref:`WEIGHTS` of a potential holds
      them (:ref:`fit_objective`).

   Training configuration
      The ``--config`` file of ``tadah train`` and ``tadah hpo``: the data,
      the descriptors, the model and the settings of the fit (format:
      :ref:`config_file`; keys: :ref:`ConfigSection`).

   Validation configuration
      The ``--validation`` file of ``tadah hpo``: the datasets on which
      an :term:`evaluation` measures its potential, without fitting them
      (:ref:`validation`).

   Target file
      The ``--hpotarget`` file of ``tadah hpo``, ``config.hpo`` by
      convention: the :term:`search space`, the :term:`loss terms <loss
      term>` and the :hpo:block:`OPTIMIZER` block
      (:ref:`nfp_target_file`).

   Block
      The lines from a line that opens with a name, such as
      :hpo:block:`!OPTIMIZER`, to the line of ``END`` and that name, such
      as ``ENDOPTIMIZER``; a target file has eight types, and the
      ``DESCRIPTOR`` blocks of a training configuration and the stage
      blocks of ``tadah refit`` take the same form (:ref:`nfp_target_file_blocks`).

   Top-level key
      A key of a :term:`target file` outside every :term:`block`.

   Search parameter
      A value of the :term:`training configuration` that a search varies
      within a box, selected by an :hpo:key:`OPTIM` line; each is one
      dimension of the :term:`search space` (:ref:`nfp_search_space`).

   Search space
      All the :term:`search parameters <search parameter>` of a target
      file, each with its box.

   Evaluation
      The scoring of one candidate, a set of values of the :term:`search
      parameters <search parameter>`: as a rule the training of one
      potential and its loss. A candidate that a check rejects before
      training (:ref:`nfp_search_space_feasibility`), or one outside the
      box, which only :hpo:opt:`CERES:STEEPEST_DESCENT` proposes, scores
      as a failed evaluation without a potential
      (:ref:`nfp_targets_failures`).

   Search
      The work of one optimiser from its start to one of its stops; a
      :term:`run` holds one search or several, such as the starts of
      :hpo:key:`MULTISTART` (:ref:`nfp_optimizer_block_strategies`).

   Run
      One invocation of ``tadah hpo``.

   Loss term
      One contribution to the loss of an :term:`evaluation`: a validation
      metric, a :hpo:key:`--varloss <PC_LAMMPS.--varloss>` variable of a
      :hpo:key:`PC_LAMMPS` line or a :hpo:key:`PC_OBSERVABLE` line
      (:ref:`nfp_targets_mean`).

   Loss-term weight
      The number that multiplies the loss function of a measured value and
      its target in a :term:`loss term` of ``tadah hpo``; a metric with
      per-dataset targets has one for each validation dataset. It is
      neither an :term:`error scale` nor a :term:`priority`
      (:ref:`nfp_targets_losses`).

   Best potential
      The potential of the :term:`evaluation` with the smallest loss of a
      run among those that did not fail, ``best_pot.tadah``
      (:ref:`nfp_output_best`).

   Error scale
      The scale that divides the residual of an observation in the sum a
      fit minimises, in the unit of the metric that measures it, set by
      :ref:`ESIGMA` (meV/atom; meV per √atom under :ref:`ENORM`
      ``SQRT``), :ref:`FSIGMA` (eV/Å) and :ref:`SSIGMA` (GPa)
      (:ref:`fit_objective`).

   Priority
      A number >= 0 that multiplies the cost of an observation in a fit: the
      ``eweight``, ``fweight`` or ``sweight`` of a structure times the
      :ref:`WDBFILE <WDBFILE>` value of its dataset (:ref:`fit_objective`).

   Balance table
      The table of a fit that gives, per quantity and dataset, the
      observations, the residuals, the share of the minimised sum and the
      effective parameters of each (:ref:`fit_balance`).

   Effective parameters
      How many of the coefficients a quantity, a dataset or the prior
      determines (:ref:`fit_balance`).

   Batch
      The evaluations that :hpo:opt:`TADAH:ANNEAL_PT`,
      :hpo:opt:`TADAH:ANNEAL_PA` or :hpo:opt:`ASA:ASA` (with
      :hpo:param:`Gener_Block_Max <PARAM.ASA:ASA.Gener_Block_Max>`
      above 1) propose together, one sequence per slot
      (:ref:`nfp_annealing_terms`), such as an epoch of
      :hpo:opt:`!TADAH:ANNEAL_PT`. Up to :hpo:key:`THREADS
      <OPTIMIZER.THREADS>` workers run them at the same time, one at a time
      under ``THREADS 1`` (:ref:`nfp_output_numbers`).

Abbreviations
-------------

.. glossary::

   API
      Application Programming Interface

   CLI
      Command Line Interface

   NFP
      Nested Fitting Procedure

   HPO
      Hyperparameter Optimisation
