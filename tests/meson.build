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

  # The in-process C unit tests compiled into tytest itself.
  test('unit', tytest, args: ['all'], timeout: 120)
endif
