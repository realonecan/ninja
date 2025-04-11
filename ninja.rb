class Ninjachat < Formula
  desc "Anonymous chat client"
  homepage "https://github.com/yourusername/ninjachat"
  url "https://github.com/realonecan/ninja/releases/download/v1.0.0/ninja-darwin-amd64.tar.gz"
  sha256 "3d6a4ae7f9c973c26ef3f706c22c04e9875aff3a93d5ec6dd9ebde3aed95dbc3" # Run `shasum -a 256 ninja-darwin-amd64.tar.gz`
  version "1.0.0"

def install
    bin.install "ninja-darwin-amd64" => "ninja"
  end

  test do
    system "#{bin}/ninja", "--version"
  end
end
