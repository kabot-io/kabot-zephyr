variable "GITHUB_SERVER_URL" {
    type    = string
    default = "https://github.com"
}

variable "GITHUB_REPOSITORY" {
    type    = string
    default = "kabot-io/kabot-zephyr"
}

target "docker-metadata-action" {}

target "devcontainer" {
    inherits = ["docker-metadata-action"]
    dockerfile = "docker/devcontainer.Dockerfile"
    context = "."
    args = {
        UBUNTU_DISTRO = "noble"
    }
}
