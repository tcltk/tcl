-- Lua filter that makes Pandoc's `man` writer produce nroff that renders
-- like the Tcl/Tk doc/*.n pages.
--
-- Usage: see markdown2nroff.tcl.
--
-- The only nroff macros this pipeline depends on (aren't
-- built in) are .BS/.BE and .CS/.CE -> see MAN_MACROS

local stringify = pandoc.utils.stringify

--------------------------------------------------------------------------
-- Pass 1: inline-level rewrites (spans + links)
--------------------------------------------------------------------------

local function has(classes, name)
  for _, c in ipairs(classes) do
    if c == name then return true end
  end
  return false
end

-- Synopsis / inline markup spans emitted by man2markdown.tcl:
--   .cmd .sub .lit .ins .ccmd   -> bold literal text
--   .arg .cargs                 -> italic placeholder text
--   .ret                        -> italic (return value)
--   .optarg  [x]{.optarg}       -> "?" + italic(x) + "?"
--   .optdot  [x]{.optdot}       -> "?" + italic(x " ...") + "?"
--   .optlit  [x]{.optlit}       -> "?" + bold(x) + "?"
local function Span(el)
  local c = el.classes
  if has(c, "cmd") or has(c, "sub") or has(c, "lit")
      or has(c, "ins") or has(c, "ccmd") then
    return pandoc.Strong(el.content)
  elseif has(c, "arg") or has(c, "cargs") or has(c, "arguments") then
    return pandoc.Emph(el.content)
  elseif has(c, "ret") then
    return pandoc.Emph(el.content)
  elseif has(c, "optarg") then
    return { pandoc.Str("?"), pandoc.Emph(el.content), pandoc.Str("?") }
  elseif has(c, "optdot") then
    local inner = {}
    for _, x in ipairs(el.content) do table.insert(inner, x) end
    table.insert(inner, pandoc.Space())
    table.insert(inner, pandoc.Str("..."))
    return { pandoc.Str("?"), pandoc.Emph(inner), pandoc.Str("?") }
  elseif has(c, "optlit") then
    return { pandoc.Str("?"), pandoc.Strong(el.content), pandoc.Str("?") }
  end
  -- Unknown/unhandled span class: leave content as-is.
  return el.content
end

-- Cross references:
--   implicit header reference (url "#foo")   -> bold, upper-cased
--   reference to another page  (url "*.md")  -> bold, as written
--   bare autolink (url == link text, a URI)  -> italic, no hyperlink
-- Inline code spans (`...`) are rendered by doctools as a quoted, bold
-- literal (".QW \"\fB...\fR\""), not a monospace font -- man devices have
-- unreliable support for \f[C], and Tcl code snippets in prose read as
-- "quoted command", not "typewriter text".
local function Code(el)
  -- Straight ASCII quotes, matching the "-smart" reader flag used
  -- elsewhere so the whole page stays visually consistent.
  return { pandoc.Str('"'), pandoc.Strong({ pandoc.Str(el.text) }), pandoc.Str('"') }
end

-- NOTE: pandoc also runs inline filters over document metadata (e.g. the
-- Copyright field, which typically contains a "<user@host>" mailto
-- autolink) -- so this needs to behave sensibly there too.
local function Link(el)
  local url = el.target
  if url:match("^mailto:") then
    return { pandoc.Str("<" .. url:gsub("^mailto:", "") .. ">") }
  elseif url:sub(1, 1) == "#" then
    return pandoc.Strong({ pandoc.Str(stringify(el.content):upper()) })
  elseif url:match("%.md$") or url:match("%.md#") then
    return pandoc.Strong(el.content)
  elseif url == stringify(el.content) then
    -- Bare autolink: italic, and left completely unescaped (a bare URL
    -- printed via the normal text path would get its hyphens rewritten
    -- to the *roff minus-sign glyph, which the original never did here).
    return pandoc.RawInline("man", "\\fI" .. url .. "\\fR")
  else
    return pandoc.Strong(el.content)
  end
end

--------------------------------------------------------------------------
-- Pass 2: block-level rewrites (headers, code blocks, ordered lists)
--------------------------------------------------------------------------

-- doctools convention: .SH NAME / .SH KEYWORDS are unquoted+uppercase;
-- every other section/subsection title is quoted+uppercase. Since nroff
-- treats a quoted single word identically to an unquoted one, the only
-- thing that actually matters for rendering is the upper-casing.
local function Header(el)
  local txt = stringify(el.content):upper()
  local macro = (el.level == 1) and "SH" or "SS"
  local line
  if txt == "NAME" or txt == "KEYWORDS" then
    line = "." .. macro .. " " .. txt
  else
    line = "." .. macro .. ' "' .. txt .. '"'
  end
  return pandoc.RawBlock("man", line)
end

-- Escape literal text destined for a no-fill (.CS/.nf) region: protect
-- backslashes and leading control characters, but leave everything else
-- (including "-") alone, matching how the original .CS/.CE blocks look.
local function escapeVerbatim(text)
  text = text:gsub("\\", "\\e")
  text = text:gsub("\n([%.'])", "\n\\&%1")
  text = text:gsub("^([%.'])", "\\&%1")
  return text
end

local function CodeBlock(el)
  -- doctools always puts a paragraph break (blank line) before a code
  -- excerpt, even right after running prose.
  local raw = ".PP\n.CS\n" .. escapeVerbatim(el.text) .. "\n.CE"
  return pandoc.RawBlock("man", raw)
end

-- doctools renders numbered lists as ".IP [N]" rather than pandoc's
-- default ".IP \"N.\" 3". Flatten each item to a Plain-only list of
-- blocks so no spurious ".PP" appears before continuation text, matching
-- the original (which never inserts one here) regardless of whether the
-- source list was "tight" or "loose".
local function OrderedList(el)
  local out = {}
  local start = el.start or 1
  for idx, item in ipairs(el.content) do
    table.insert(out, pandoc.RawBlock("man", ".IP [" .. (start + idx - 1) .. "]"))
    for _, blk in ipairs(item) do
      if blk.t == "Para" then
        table.insert(out, pandoc.Plain(blk.content))
      else
        table.insert(out, blk)
      end
    end
  end
  return out
end

--------------------------------------------------------------------------
-- Pass 3: turn the {.synopsis} Div into a literal .nf/.fi block, using
-- Pandoc's own writer (via pandoc.write) to render each already-styled
-- line, so escaping/font-code generation stays consistent with the rest
-- of the document. SoftBreaks *within* a synopsis paragraph mark
-- one-command-per-line boundaries and must become real newlines (not
-- reflowed text, and not pandoc's LineBreak, which itself expands to
-- ".PD 0 / .P / .PD" -- inappropriate inside .nf).
--------------------------------------------------------------------------

local function renderInlineMan(inlines)
  local text = pandoc.write(pandoc.Pandoc({ pandoc.Plain(inlines) }), "man",
    { wrap_text = "none" })
  return (text:gsub("%s+$", ""))
end

local function synopsisDiv(el)
  local lines = {}
  for i, blk in ipairs(el.content) do
    if i > 1 then table.insert(lines, false) end
    if blk.t == "Para" or blk.t == "Plain" then
      local current = {}
      for _, inline in ipairs(blk.content) do
        if inline.t == "SoftBreak" then
          table.insert(lines, current)
          current = {}
        else
          table.insert(current, inline)
        end
      end
      table.insert(lines, current)
    end
  end

  local textlines = { ".nf" }
  for _, l in ipairs(lines) do
    if l == false then
      table.insert(textlines, "")
    else
      table.insert(textlines, renderInlineMan(l))
    end
  end
  table.insert(textlines, ".fi")
  return pandoc.RawBlock("man", table.concat(textlines, "\n"))
end

local function Div(el)
  if has(el.classes, "synopsis") then
    return synopsisDiv(el)
  end
  return nil
end

--------------------------------------------------------------------------
-- Pass 4: whole-document wrap-up -- comment header, .TH, the minimal
-- .BS/.BE/.CS/.CE macro definitions, .BS/.BE around NAME+SYNOPSIS, and
-- the SEE ALSO / KEYWORDS trailer synthesized from YAML metadata.
--------------------------------------------------------------------------

-- YAML "true"/"false"/"y"/"n" scalars are parsed as MetaBool, not text
-- (e.g. "ManualSection: n" -- a real hazard here, since "n" is exactly
-- the section value most Tcl manual pages use).
local function metaText(value)
  if type(value) == "boolean" then
    return value and "y" or "n"
  elseif type(value) == "string" then
    return value
  else
    return stringify(value)
  end
end

local function metaStr(meta, key, default)
  if meta[key] ~= nil then return metaText(meta[key]) end
  return default
end


local MAN_MACROS = [[
.nr ^l \n(.l
.de BS
.br
.mk ^y
.nr ^b 1u
.if n .nf
.if n .ti 0
.if n \l'\\n(.lu\(ul'
.if n .fi
..
.de BE
.nf
.ti 0
.mk ^t
.ie n \l'\\n(^lu\(ul'
.el \{\
.ie !\\n(^b-1 \{\
\h'-1.5n'\L'|\\n(^yu-1v'\l'\\n(^lu+3n\(ul'\L'\\n(^tu+1v-\\n(^yu'\l'|0u-1.5n\(ul'
.\}
.el \}\
\h'-1.5n'\L'|\\n(^yu-1v'\h'\\n(^lu+3n'\L'\\n(^tu+1v-\\n(^yu'\l'|0u-1.5n\(ul'
.\}
.\}
.fi
.br
.nr ^b 0
..
.de CS
.RS
.nf
.ta .25i .5i .75i 1i
..
.de CE
.fi
.RE
..]]

local function metaList(meta, key)
  local out = {}
  if meta[key] then
    for _, item in ipairs(meta[key]) do
      table.insert(out, metaText(item))
    end
  end
  return out
end

local function Pandoc(doc)
  local meta = doc.meta

  local cmdname = metaStr(meta, "CommandName", "")
  local section = metaStr(meta, "ManualSection", "n")
  local version = metaStr(meta, "Version", "")
  if version:lower() == "unknown" then version = "" end
  local tclpart = metaStr(meta, "TclPart", "Tcl")
  local tcldesc = metaStr(meta, "TclDescription", "")
  local srcfile = metaStr(meta, "SourceFile", cmdname .. ".md")
  local copyrightLines = metaList(meta, "Copyright")
  local keywords = metaList(meta, "Keywords")
  local links = metaList(meta, "Links")

  local header = {}
  table.insert(header, "'\\\"")
  table.insert(header, "'\\\" Generated from file '" .. srcfile .. "' by Pandoc")
  table.insert(header,
    "'\\\" This file is not meant to be edited. Edit the markdown source instead.")
  table.insert(header, "'\\\"")
  for _, c in ipairs(copyrightLines) do
    table.insert(header, "'\\\" " .. c)
  end
  table.insert(header, "'\\\"")
  table.insert(header,
    "'\\\" See the file \"license.terms\" for information on usage and redistribution")
  table.insert(header, "'\\\" of this file, and for a DISCLAIMER OF ALL WARRANTIES.")
  table.insert(header, "'\\\"")
  table.insert(header, '.TH "' .. cmdname .. '" ' .. section .. ' "' ..
    version .. '" ' .. tclpart .. ' "' .. tcldesc .. '"')
  table.insert(header, MAN_MACROS)

  -- Splice .BS before ".SH NAME" and .BE right after the following
  -- .nf/.fi synopsis block (the standard doctools NAME+SYNOPSIS layout).
  local body = {}
  local bsDone, beDone, afterName = false, false, false
  for _, b in ipairs(doc.blocks) do
    if not bsDone and b.t == "RawBlock" and b.format == "man" and b.text == ".SH NAME" then
      table.insert(body, pandoc.RawBlock("man", ".BS"))
      table.insert(body, pandoc.RawBlock("man",
        "'\\\" Note:  do not modify the .SH NAME line immediately below!"))
      table.insert(body, b)
      bsDone = true
      afterName = true
    elseif afterName and b.t == "Para" then
      -- doctools never puts a .PP between ".SH NAME" and the one-line
      -- name/description text.
      table.insert(body, pandoc.Plain(b.content))
      afterName = false
    else
      afterName = false
      table.insert(body, b)
    end
    if bsDone and not beDone and b.t == "RawBlock" and b.format == "man"
        and b.text:match("^%.nf") then
      table.insert(body, pandoc.RawBlock("man", ".BE"))
      beDone = true
    end
  end

  local out = {}
  table.insert(out, pandoc.RawBlock("man", table.concat(header, "\n")))
  for _, b in ipairs(body) do table.insert(out, b) end

  if #links > 0 then
    table.insert(out, pandoc.RawBlock("man", '.SH "SEE ALSO"'))
    table.insert(out, pandoc.RawBlock("man", table.concat(links, ", ")))
  end
  if #keywords > 0 then
    table.insert(out, pandoc.RawBlock("man", ".SH KEYWORDS"))
    table.insert(out, pandoc.RawBlock("man", table.concat(keywords, ", ")))
  end

  return pandoc.Pandoc(out, doc.meta)
end

return {
  { Span = Span, Link = Link, Code = Code },
  { Header = Header, CodeBlock = CodeBlock, OrderedList = OrderedList },
  { Div = Div },
  { Pandoc = Pandoc },
}
