class Ninja < Formula
  desc "Anonymous chat client"
  homepage "https://github.com/yourusername/ninjachat"
  url "https://github.com/realonecan/ninjachat/releases/download/v1.0.0/ninja-darwin-amd64.tar.gz"
  sha256 "your-sha256-checksum-here"
  version "1.0.0"

  def install
    bin.install "ninja"
  end

  test do
    system "#{bin}/ninja", "--version"
  end
end
