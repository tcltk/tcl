#!/bin/bash

./helper.pl --update-makefiles || exit 1

makefiles=(makefile makefile.shared makefile_include.mk makefile.msvc makefile.unix makefile.mingw)
vcproj=(libtommath_VS2008.vcproj)

if [ $# -eq 1 ] && [ "$1" == "-c" ]; then
  git add ${makefiles[@]} ${vcproj[@]} && git commit -m 'Update makefiles'
fi

exit 0

# ref:         $Format:%D$
# git commit:  $Format:%H$
# commit time: $Format:%ai$
