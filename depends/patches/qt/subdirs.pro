#
# This project file enable the Qt depends recipe to build additional modules
# with proper dependency and path resolution.
#
# https://wiki.qt.io/SUBDIRS_-_handling_dependencies
#

cache(, super)

TEMPLATE = subdirs

SUBDIRS = \
  qtbase \
  qtsvg \
  qttools \
  qttranslations

qtbase.target = module-qtbase

qtsvg.target = module-qtsvg
qtsvg.depends = qtbase

qttools.target = module-qttools
qttools.depends = qtbase

qttranslations.target = module-qttranslations
qttranslations.depends = qttools

load(qt_configure)
