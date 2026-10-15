.. _anneal_vocabulary:

Annealing terminology
=====================

The annealing references and ``hpo_run.log`` distinguish objective
evaluations from the algorithm's units of work.

.. glossary::

   eval
      One call to evaluate the objective. Annealing's ``MAXEVAL`` counts
      the seed, temperature probes, population initialisation and search
      proposals. Separate ``NOISE CALIBRATION`` repeats and parallel
      tempering's ``INIT_MODE JITTER AUTO`` probes are outside that
      search budget, so the journal's total can be larger.

   proposal
      A candidate point submitted to the Metropolis acceptance rule.
      An evaluated proposal costs one objective evaluation whether it is
      accepted or rejected. Redrawing a coordinate to keep the proposal
      inside its bounds does not itself evaluate the objective.

   sweep
      In parallel tempering, one whole-vector proposal with the CLASSIC
      kernel, or one proposal for each dimension with the Corana kernel.
      ``ANNEAL_PA`` uses the key ``SWEEPS`` for a different unit: the
      number of complete temperature rungs per walker at a temperature.

   rung
      A serial or population-annealing chain's work at one temperature:
      ``CHAIN_LENGTH`` proposals for CLASSIC, or ``NT * NS * dimensions``
      proposals for Corana. A budget or stop can interrupt a rung.

   epoch
      One parallel-tempering iteration: every replica runs one sweep at
      its slot's temperature. If the exchange cadence is reached, the
      driver then attempts exchanges.

   exchange round
      A parallel-tempering set of exchange attempts between disjoint
      neighbouring slots. Rounds alternate pairings: slots 1–2, 3–4,
      and so on in one round; slots 2–3, 4–5, and so on in the next.
      ``SWAP_EVERY`` sets the number of epochs between rounds.

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
      A parallel-tempering diagnostics interval of ``SWAP_WINDOW``
      epochs. With ``SWAP_WINDOW 0``, reports use cumulative statistics;
      an enabled best-stall stopping test still uses the default interval
      of 20 epochs. Its ``NEPS`` counts consecutive stopping windows.
