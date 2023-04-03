import configparser as cfgp, re
import glob
from dataclasses import dataclass
from typing import Any, MutableMapping
from collections import ChainMap

class Tstr:
	def __init__(self, template: str):
		self.template = template
		self.pattern = re.compile(r'\$\{([a-zA-Z_][a-zA-Z0-9_.]*)\s*(\:\s*([^\}]*)\s*)?\}')

	def eval(
		self,
	  local_ns: MutableMapping[str, Any],
		global_ns: MutableMapping[str, Any] = {}
	):
		ns = ChainMap(local_ns, global_ns)
		def sub(match: re.Match[str]):
			name: str = match[1]
			assert name is not None
			if match[3]: # has argument match, is a function.
				if name not in ns:
					raise KeyError(f'no such function: \'{name}\'')
				func = ns[name]
				if not callable(func):
					raise ValueError(f'\'{name}\' is not callable', func)
				return func(match[3], ns)
			else: # basic variable.
				if name not in ns:
					return match[0]
				return ns[name]
		return self.pattern.sub(sub, self.template)


def global_ns_glob(pat: str, ns: MutableMapping[str, Any]) -> str:
	return ' '.join(glob.glob(Tstr(pat).eval(ns), recursive=True))

global_ns = { 'glob': global_ns_glob }

p = cfgp.ConfigParser()
p.read("build.cfg")

includes: list[str] = \
	p["meta"]["includes"].split()

rulenames: list[str] = \
	p["meta"]["rulenames"].split()

defines: dict[str, str] = {}
for item in p["globals"]:
	t: Tstr = Tstr(p["globals"][item])
	defines[item] = t.eval(defines, global_ns)


@dataclass
class Rule:
	ins: list[str]
	pat: str | None
	out: list[str] | None
	
	@property
	def has_outs(self) -> bool:
		return not (self.pat is None and
			          self.out is None)

	@property
	def outs(self) -> list[str] | None:
		if self.pat is None:
			if self.out is None:
				return None
			return self.out
		
		l: list[str] = []
		t = Tstr(self.pat)
		for in_ in self.ins:
			l.append(t.eval({ "in": in_ }, global_ns))
		return l


rules = { name: Rule([], None, None)
          for name in rulenames }
ns: dict[str, str] = {**defines}

for rule in p["rules"]:
	rule: str
	name, opt = rule.split('.')
	v: str = p["rules"][rule]

	if name not in rules:
		print(f"bad rule {name} (in {name}.{opt})")
		exit(1)

	r = rules[name]

	match opt:
		case 'ins':
			r.ins = []
			y: str = Tstr(v).eval(ns, global_ns).strip()
			if len(y) > 0:
				r.ins.extend(y.split())
				
		case 'out':
			r.out = []
			y: str = Tstr(v).eval(ns, global_ns).strip()
			if len(y) > 0:
				r.out.extend(y.split())
				
		case 'pat':
			r.pat = v
		case _:
			print(f"bad opt {opt} (in {name}.{opt})")
			exit(1)
	
	if r.ins is not None:
		ns[f"{name}.ins"] = ' '.join(r.ins)

	if r.has_outs:
		os: list[str] = r.outs # type: ignore
		ns[f"{name}.out"] = ' '.join(os)

	if r.pat is not None:
		ns[f"{name}.pat"] = r.pat

with open('build.ninja', 'w') as fout:
	for include in includes:
		fout.write(f'include {include}\n')
	fout.write('\n')

	for name, value in defines.items():
		v = value.replace('\n', ' $\n')
		fout.write(f'{name} = {v}\n')
	fout.write('\n')

	for name, r in rules.items():
		os = r.outs # type: ignore
		if os is None:
			print(f"can't compute outs of {name}")
			exit(1)
		if len(os) == 1:
			fout.write(f'build {os[0]}: {name}')
			for i in r.ins:
				fout.write(f' {i}')
			fout.write('\n')
		else:
			for i, o in zip(r.ins, os):
				fout.write(f'build {o}: {name} {i}\n')
	fout.write('\n')
	
	
