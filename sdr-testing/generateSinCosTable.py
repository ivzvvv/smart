import math as m

# 100 Hz : 8 000 000 / 100 = 80 000 values
# Step : 2*pi / 80 000 = pi / 40 000
step = m.pi / 40000

with open("table.txt", "w") as table:
    table.write('table = {')
    for i in range(0, 80000):
        table.write(f'{m.sin(i*step):.6f}')
        if i == 80000-1:
            table.write('};')
        else:
            table.write(',')
    