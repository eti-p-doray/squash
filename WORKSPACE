http_archive(
  name = "com_google_googletest",
  urls = ["https://github.com/google/googletest/archive/master.zip"],
  strip_prefix = "googletest-master",
)

http_archive(
  name = "com_google_absl",
  urls = ["https://github.com/abseil/abseil-cpp/archive/master.zip"],
  strip_prefix = "abseil-cpp-master",
)

new_local_repository(
  name = "chromium",
  path = "third_party/chromium",
  build_file = "third_party/chromium.BUILD",
)


new_local_repository(
  name = "easyloggingpp",
  path = "third_party/easyloggingpp_v9.95.3",
  build_file = "third_party/easyloggingpp.BUILD",
)

