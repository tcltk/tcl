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

-- every line of a synopsis block becomes its own paragraph
-- (Pandoc would otherwise join the lines into one paragraph),
-- so that the rows can be spaced apart with CSS

function Div(el)
  if not el.classes:includes("synopsis") then return nil end
  local out = pandoc.List()
  for _, blk in ipairs(el.content) do
    if blk.t == "Para" or blk.t == "Plain" then
      local row = pandoc.List()
      for _, inl in ipairs(blk.content) do
        if inl.t == "SoftBreak" or inl.t == "LineBreak" then
          out:insert(pandoc.Para(row))
          row = pandoc.List()
        else
          row:insert(inl)
        end
      end
      if #row > 0 then out:insert(pandoc.Para(row)) end
    else
      out:insert(blk)
    end
  end
  el.content = out
  return el
end
