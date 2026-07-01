import matplotlib.pyplot as plt
import csv

steps, frag, pressure, reuse = [], [], [], []

with open("output.csv", "r") as f:
    reader = csv.DictReader(f)
    for row in reader:
        steps.append(int(row["Step"]))
        frag.append(float(row["Frag_MB"]))
        pressure.append(float(row["Pressure"]))
        reuse.append(float(row["ReuseRate"]))

plt.figure(figsize=(15,4))

plt.subplot(1,3,1)
plt.plot(steps, frag)
plt.title("Fragmentation (MB)")

plt.subplot(1,3,2)
plt.plot(steps, pressure)
plt.title("Memory Pressure")

plt.subplot(1,3,3)
plt.plot(steps, reuse)
plt.title("Page Reuse Rate")

plt.tight_layout()
plt.show()