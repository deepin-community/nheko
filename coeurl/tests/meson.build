doctest_dep = dependency('doctest', fallback : ['onqtam-doctest', 'doctest_dep'])

requests = executable('requests', ['requests.cpp'],
  cpp_args : '-DDOCTEST_CONFIG_IMPLEMENT_WITH_MAIN',
  dependencies: [coeurl_dep, doctest_dep],
  install : false)

test('requests', requests)

requests_tls = executable('requests_tls', ['requests_tls.cpp'],
  cpp_args : '-DDOCTEST_CONFIG_IMPLEMENT_WITH_MAIN',
  dependencies: [coeurl_dep, doctest_dep],
  install : false)

test('requests_tls', requests_tls)


