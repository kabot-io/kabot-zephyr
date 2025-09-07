#!/usr/bin/zsh

# nuke files ignored by git

git clean -X --force --force --interactive -d
