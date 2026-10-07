-- Lua filter that makes Pandoc's `html` writer produce html that renders
-- links to the Tcl/Tk doc/.../*.md pages as html links
--
-- Used from markdown2html.tcl

function Link(el)
  -- leave http: etc. alone
  if not el.target:match("^%a+:") then
    el.target = (el.target:gsub("%.md$", ".html"):gsub("%.md#", ".html#"))
  end
  return el
end