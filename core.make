; @file
; Makefile for making a new Drupal site using this profile.

api = 2
core = 7.x

projects[drupal][version] = 7.31

projects[odprofile][type] = profile
projects[odprofile][download][type] = git
projects[odprofile][download][url] = foo
projects[odprofile][download][branch] = develop
