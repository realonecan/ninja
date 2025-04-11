class Ninja < Formula
  desc "Anonymous chat client"
  homepage "https://github.com/yourusername/ninja"
  url "https://github.com/realonecan/ninja/releases/download/v1.0/ninja-darwin-amd64.tar.gz"
  sha256 "YOUR_SHA256" # Run `shasum -a 256 ninja-darwin-amd64.tar.gz`
  version "1.0"

  def install
    bin.install "ninja-chat" => "ninja"
  end
