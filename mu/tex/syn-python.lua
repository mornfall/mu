if not modules then modules = { } end modules ['syn-haskell'] = {
    version   = 1.001,
    comment   = "haskell syntax highlighter",
    author    = "Petr Ročkai",
    copyright = "PRAGMA ADE / ConTeXt Development Team",
    license   = "see context related readme files"
}

local P, S, V, patterns = lpeg.P, lpeg.S, lpeg.V, lpeg.patterns

local context                      = context
local verbatim                     = context.verbatim
local makepattern                  = visualizers.makepattern
local makenested                   = visualizers.makenested

local SynSnippet                   = context.SynSnippet
local startSynSnippet              = context.startSynSnippet
local stopSynSnippet               = context.stopSynSnippet

local SynSnippetString              = verbatim.SynSnippetString
local SynSnippetKeyword             = verbatim.SynSnippetKeyword
local SynSnippetName                = verbatim.SynSnippetName
local SynSnippetType                = verbatim.SynSnippetType
local SynSnippetComment             = verbatim.SynSnippetComment
local SynSnippetMagic               = verbatim.SynSnippetMagic
local SynSnippetOperator            = verbatim.SynSnippetOperator
local SynSnippetBoundary            = verbatim.SynSnippetBoundary
local SynSnippetQuote               = verbatim.SynSnippetQuote

local keywords, plain, metafun

local function initialize()
    local kw = { "try", "except", "if", "else", "elif", "while",
                 "for", "in", "continue", "return", "assert",
                 "from", "import", "def", "pass",
                 "yield", "await", "raise", "lambda", "finally", "with", "as", "is" }
    local ty = { "int", "float", "str", "list", "dict", "set", "range", "len", "chr", "ord" }
    keywords = table.tohash(kw)
    types    = table.tohash(ty)
end

local function visualizename(s)
    if not keywords then
        initialize()
    end
    if keywords[s] then
        SynSnippetKeyword(s)
    elseif types[s] then
        SynSnippetType(s)
    elseif s == "and" or s == "or" then
        SynSnippetOperator(s)
    else
        SynSnippetName(s)
    end
end

local function comment(content,equals,settings)
    SynSnippetComment(content)
    -- visualizers.write(content,settings) -- unhandled
end

local handler = visualizers.newhandler {
    startinline  = function() SynSnippet(false,"{") end,
    stopinline   = function() context("}") end,
    startdisplay = function() startSynSnippet() end,
    stopdisplay  = function() stopSynSnippet() end ,
    operator     = function(s) SynSnippetOperator(s) end,
    magic        = function(s) SynSnippetMagic(s) end,
    comment      = function(s) SynSnippetComment(s) end,
    string       = function(s) SynSnippetString(s) end,
    quote        = function(s) SynSnippetQuote(s) end,
    boundary     = function(s) SynSnippetBoundary(s) end,
    name         = visualizename,
}

local name        = (patterns.letter + S("_"))^1
local magic       = S("#@")
local boundary    = S('()[]<>;"|$\\{},') + P("::")
local operator    = S("!=-+/*`?^&%.:~")
  
local grammar = visualizers.newgrammar("default", { "visualizer",

    comment     = ((P("#") * (P(1) - P("\n"))^0)) / comment,
    string      = makepattern(handler,"quote",patterns.dquote)
                * makepattern(handler,"string",patterns.nodquote)
                * makepattern(handler,"quote",patterns.dquote)
                + makepattern(handler, "string", (P("'") ) * ( P(1) - P("'") )^0 * P("'") ),
    name        = makepattern(handler,"name",name),
    magic       = makepattern(handler,"magic",magic),
    boundary    = makepattern(handler,"boundary",boundary),
    operator    = makepattern(handler,"operator",operator),

    pattern     =
        V("comment") + V("string") + V("name") + V("magic") + V("boundary") + V("operator")
      + V("newline") * V("emptyline")^0 * V("beginline")
      + V("space")
      + V("default"),

    visualizer  =
        V("pattern")^1

} )

local parser = P(grammar)

visualizers.register("python", { parser = parser, handler = handler, grammar = grammar } )
