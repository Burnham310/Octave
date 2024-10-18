## BNF

```
<Pgm>		::= <Decl>+ 
<Decl> 		::= <Ident> = <Expr> 
<Ident>		::= [a-z_][a-z_<Digit>]*
<Qual>		::= [so#b]+'
<Expr>		::= <Ident> 
              	| <Int> 
				| <Expr> .+
                | <Qual> <Expr>
                | <Int> ' <Expr>
                | [<Expr>*]
                | |{<Decl>}:{<Decl>}: <Expr>*|
                | Expr & Expr
                
                



<Digit>		::= [0-9]
<Int>		::= [1-9][0-9]*

{} is comma seperated list
```