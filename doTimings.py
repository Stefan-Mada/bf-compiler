from os import listdir
import subprocess
from os.path import isfile, join
import pandas as pd
import time

onlyfiles = [f for f in listdir("benches") if isfile(join("benches", f))]
cliOptions = ["", "--partial-eval false"]

df = pd.DataFrame(data=onlyfiles)

for idx, option in enumerate(cliOptions):
  timings = []
  
  for file in onlyfiles:
    subprocess.run(
      ['./compiler.out', f"benches/{file}", "-o", "scriptasm.s"] + option.split()
    )
    subprocess.run(
      ['clang', "scriptasm.s", "-o", "scriptasm.out"]
    )
    
    time_start = time.time()
    result = subprocess.run(
      ['time', './scriptasm.out'],
      capture_output=subprocess.DEVNULL,
      universal_newlines=True,
      encoding="cp437"
    )
    time_elapsed = time.time() - time_start
    timings.append(time_elapsed)
  df.insert(idx + 1, option, timings)

print(df)