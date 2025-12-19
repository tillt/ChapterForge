# Homebrew tap formula (HEAD-only for now).
class Chapterforge < Formula
  desc "Mux AAC/M4A with Apple-compatible chapter text, URLs, and images"
  homepage "https://github.com/tillt/ChapterForge"
  head "https://github.com/tillt/ChapterForge.git"

  depends_on "cmake" => :build
  depends_on "ninja" => :build
  depends_on "pkg-config" => :build
  depends_on "nlohmann-json"

  def install
    system "cmake", "-S", ".", "-B", "build", "-G", "Ninja",
           "-DCMAKE_BUILD_TYPE=Release"
    system "cmake", "--build", "build", "--target", "chapterforge_cli"

    bin.install "build/chapterforge_cli"
    lib.install "build/libchapterforge.a" if File.exist?("build/libchapterforge.a")

    include.install Dir["include/*.hpp"]
    version_header = "build/generated/chapterforge_version.hpp"
    include.install version_header if File.exist?(version_header)
  end

  test do
    system "#{bin}/chapterforge_cli", "--help"
  end
end
