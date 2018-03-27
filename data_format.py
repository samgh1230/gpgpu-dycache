f = open("a.txt","r")
output = open("a.csv","w")
for line in f:
    if(line.startswith("a")):
        line = line.strip()
        mark,start_vertex, end_vertex,weight = line.split(" ")
        output.write(str(start_vertex)+" "+str(end_vertex))
f.close()
output.close()