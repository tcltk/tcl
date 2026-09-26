package require Tcl 9

# This script is a prototype/placeholder for converting markdown-formatted manual pages 
# (as produced by man2markdown.tcl) into HTML-formatted manual pages,
# using Pandoc.

exec \
	pandoc -f markdown-tex_math_dollars-smart -t html -s -c tcl-docs.css -o ../doc/html/string.html ../doc/markdown/string.md

exec \
	pandoc -f markdown-tex_math_dollars-smart -t html -s -c tcl-docs.css -o ../doc/html/dict.html ../doc/markdown/dict.md

exec \
	pandoc -f markdown-tex_math_dollars-smart -t html -s -c tcl-docs.css -o ../doc/html/chan.html ../doc/markdown/chan.md