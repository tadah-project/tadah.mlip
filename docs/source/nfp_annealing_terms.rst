.. _anneal_vocabulary:

Annealing Terminology
=====================

This page defines the units of work that the annealing optimisers count,
so that the evaluation budget and the counts in ``hpo_run.log`` can be
read. It builds on the pages :doc:`anneal`, :doc:`anneal_pt` and
:doc:`anneal_pa`, and on :ref:`output_files`.

.. glossary::

   evaluation
      One candidate trained and scored: one computation of the loss.
      For an annealing optimiser, :hpo:key:`MAXEVAL` counts the seed,
      temperature probes, population initialisation and search
      proposals. Separate :hpo:key:`CALIBRATION <NOISE.CALIBRATION>`
      repeats of the :hpo:block:`NOISE` block and the probes of
      :hpo:param:`INIT_MODE` JITTER ``AUTO`` of
      :hpo:opt:`TADAH:ANNEAL_PT` are outside that search budget, so the
      journal's total can be larger.

   proposal
      A candidate point submitted to the Metropolis acceptance rule.
      An evaluated proposal costs one evaluation whether it is
      accepted or rejected. Redrawing a coordinate to keep the proposal
      inside its bounds only generates a candidate point.

   sweep
      In parallel tempering, one whole-vector proposal with the CLASSIC
      kernel, or one proposal for each dimension with the Corana kernel.
      :hpo:opt:`TADAH:ANNEAL_PA` uses the key :hpo:param:`SWEEPS` for a different unit: the
      number of complete temperature rungs per walker at a temperature.

   rung
      A serial or population-annealing chain's work at one temperature:
      :hpo:param:`CHAIN_LENGTH` proposals for CLASSIC, or
      :hpo:param:`NT` * :hpo:param:`NS` * dimensions proposals for
      Corana. A budget or stop can interrupt a rung.

   epoch
      One parallel-tempering iteration: every replica runs one sweep at
      its slot's temperature. After every :hpo:param:`SWAP_EVERY` epochs,
      the optimiser then attempts an exchange round.

   exchange round
      A parallel-tempering set of exchange attempts between disjoint
      neighbouring slots. Rounds alternate pairings: slots 1–2, 3–4,
      and so on in one round; slots 2–3, 4–5, and so on in the next.
      :hpo:param:`!SWAP_EVERY` sets the number of epochs between rounds.

   pair
      Two adjacent temperature slots. Exchange statistics are recorded
      separately for each neighbouring pair.

   slot
      A position on the temperature ladder. The journal numbers slots
      from 1 (coldest) to the number of replicas (hottest). Proposal-step
      calibration and the slot's best remain with that position when
      states exchange. Optional ladder adaptation can change interior
      slot temperatures.

   replica
      A chain running at one temperature slot in parallel tempering.

   chain
      A Metropolis walk with a current parameter vector, loss and
      proposal-step state.

   walker
      In parallel tempering, a state lineage followed through exchanges;
      a round trip follows it from cold to hot and back. In population
      annealing, one population member whose point, loss and proposal
      steps can be copied into descendants during resampling.

   window
      A parallel-tempering diagnostics interval of
      :hpo:param:`SWAP_WINDOW` epochs. With ``SWAP_WINDOW OFF``, reports
      use cumulative statistics; the stop set by
      :hpo:key:`FTOL_REL <OPTIMIZER.FTOL_REL>` or
      :hpo:key:`FTOL_ABS <OPTIMIZER.FTOL_ABS>` still uses windows of 20
      epochs. :hpo:param:`NEPS` is the number of consecutive windows it
      compares.
