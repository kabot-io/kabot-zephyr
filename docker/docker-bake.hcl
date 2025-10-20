variable "GITHUB_SERVER_URL" {
    type    = string
    default = "https://github.com"
}

variable "GITHUB_REPOSITORY" {
    type    = string
    default = "kabot-io/kabot-zephyr"
}

target "docker-metadata-action" {}

target "images" {
    inherits = ["docker-metadata-action"]
    name = "dev-${ubuntu_distro}-${ros_distro}"
    matrix = {
        ubuntu_distro = ["noble"]
        ros_distro    = ["jazzy", "kilted", "rolling"]
    }
    dockerfile = "docker/devcontainer.Dockerfile"
    context = "."
    args = {
        UBUNTU_DISTRO = "${ubuntu_distro}"
        ROS_DISTRO    = "${ros_distro}"
    }
}
