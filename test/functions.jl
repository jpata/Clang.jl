using Clang.cindex
using Base.Test

top = cindex.parse_header("cxx/cbasic.h";cplusplus = true)

funcs = cindex.search(top, "func")
@test length(funcs) == 1
f = funcs[1]

#@test map(spelling, cindex.function_args(f)) == ASCIIString["Int", "Int"]
@test map(spelling, cindex.function_args(f)) == ASCIIString["x", "y"]
@test isa(return_type(f), IntType)

defs = cindex.function_arg_defaults(f)
@test defs == (nothing, 1)


funcs = cindex.search(top, "func2")
@test length(funcs) == 1
f = funcs[1]

function get_modifiers(p::ParmDecl)
	modifs = Symbol[]
	for tok in tokenize(p)
		
		#only read keywords
		isa(tok, cindex.Keyword) || continue

		#read up to variable identifier
		isa(tok, cindex.Identifier) && break

		const tt = tok.text
		if tt in ["const", "virtual"]
			push!(modifs, symbol(tt))
		end
	end
	return modifs
end

for a in cindex.function_args(f)
	println(spelling(a), " ", cu_type(a))
	println(get_modifiers(a))
end
return_type(f)|>pointee_type|>println



