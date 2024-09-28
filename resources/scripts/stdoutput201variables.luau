local Str = ""

for VariableNumber = 1, 200 do
	Str ..= `\nlocal Var{VariableNumber} = 'HI'..tostring({VariableNumber})\nprint(Var{VariableNumber})`
end

Str ..= `\nlocal TheStraw = "That breaks the camel's back"`

print(Str)