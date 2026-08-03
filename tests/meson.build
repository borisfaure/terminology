if tests
  run_tests = find_program('run_tests.sh')

  # Each script's output is piped through tytest and the resulting state
  # checksum compared against tests.results.
  test('escape-codes',
       run_tests,
       args: ['-v',
              '-t', tytest.full_path(),
              '-r', meson.current_source_dir() / 'tests.results',
              '-d', meson.current_source_dir()],
       depends: tytest,
       workdir: meson.current_source_dir(),
       timeout: 300)

  # Replayed with tiny reads, so every escape sequence and every multibyte
  # character straddles a read boundary. Results must match the run above.
  foreach chunk : ['1', '3', '7']
    test('escape-codes-chunk' + chunk,
         run_tests,
         args: ['-v',
                '-t', tytest.full_path(),
                '-r', meson.current_source_dir() / 'tests.results',
                '-d', meson.current_source_dir(),
                '--chunk=' + chunk],
         depends: tytest,
         workdir: meson.current_source_dir(),
         timeout: 600)
  endforeach

  # Again with the vector kernels off, so the scalar path -- what non-NEON
  # targets run -- has end-to-end coverage too.
  test('escape-codes-scalar',
       run_tests,
       args: ['-v',
              '-t', tytest.full_path(),
              '-r', meson.current_source_dir() / 'tests.results',
              '-d', meson.current_source_dir()],
       env: {'TERMINOLOGY_SIMD_DISABLE': '1'},
       depends: tytest,
       workdir: meson.current_source_dir(),
       timeout: 300)

  # The in-process C unit tests compiled into tytest itself.
  test('unit', tytest, args: ['all'], timeout: 120)
  test('unit-scalar', tytest, args: ['all'],
       env: {'TERMINOLOGY_SIMD_DISABLE': '1'}, timeout: 120)
endif

if benchmarks
  python3 = find_program('python3')
  bench_corpus_dir = meson.current_build_dir()

  # Generated rather than committed: megabytes of derived data, and the
  # generator is deterministic so the bytes are reproducible. It writes to
  # @OUTDIR@ so that what it produces is what 'output' declares.
  bench_corpus = custom_target('bench-corpus',
      output: ['plain-ascii', 'scroll', 'sgr', 'unicode'],
      command: [python3,
                meson.current_source_dir() / 'bench' / 'gen_corpus.py',
                '@OUTDIR@'],
      build_by_default: false)

  # Run with `meson test --benchmark`.
  benchmark('pty-intake',
            tybench,
            args: ['-i', '5',
                   bench_corpus_dir / 'plain-ascii',
                   bench_corpus_dir / 'scroll',
                   bench_corpus_dir / 'sgr',
                   bench_corpus_dir / 'unicode'],
            depends: bench_corpus,
            timeout: 600)
endif
