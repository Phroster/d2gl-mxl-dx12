from pathlib import Path
import sys,struct,subprocess,posixpath,re,json
root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/"external/mpq-python"))
import mpyq
archive=mpyq.MPQArchive(str(root/"d2gl/d2gl.mpq"),listfile=False)
out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
blast=Path(sys.argv[2])
def read(name):
    name=posixpath.normpath(name.replace("\\","/"))
    assert name.startswith("data/shaders/"),name
    entry=archive.get_hash_table_entry(name.replace("/","\\"))
    if not entry:raise RuntimeError("Missing MPQ file: "+name)
    block=archive.block_table[entry.block_table_index]
    assert not block.flags&0x10000,("Encrypted shader",name)
    archive.file.seek(archive.header["offset"]+block.offset)
    data=archive.file.read(block.archived_size)
    if block.flags&0x100:
        size=512<<archive.header["sector_size_shift"];count=(block.size+size-1)//size
        offsets=struct.unpack("<"+"I"*(count+1),data[:4*(count+1)])
        chunks=[]
        for i in range(count):
            chunk=data[offsets[i]:offsets[i+1]]
            expected=min(size,block.size-i*size)
            if len(chunk)<expected:
                result=subprocess.run([str(blast)],input=chunk,capture_output=True,creationflags=0x08000000)
                if result.returncode:raise RuntimeError(("PKWare decode failed",name,i))
                chunk=result.stdout
            assert len(chunk)==expected,(name,len(chunk),expected)
            chunks.append(chunk)
        data=b"".join(chunks)
    elif block.flags&0x200:
        data=archive.read_file(name.replace("/","\\"))
    assert len(data)==block.size,(name,len(data),block.size)
    try:return data.decode("utf-8-sig")
    except UnicodeDecodeError:return data.decode("cp1252")
def includes(name,stack=()):
    assert name not in stack,("Include cycle",name)
    text=read(name)
    def expand(m):
        child=posixpath.normpath(posixpath.join(posixpath.dirname(name),m[1].replace("\\","/")))
        return includes(child,(*stack,name))
    return re.sub(r'^\s*#include\s+"([^"]+)"[^\n]*',expand,text,flags=re.M)
entries=[];done=set()
presets=read("data/shaders/list.txt").splitlines()
for preset in presets:
    preset=preset.strip()
    if not preset or preset.startswith("#"):continue
    path=posixpath.normpath("data/shaders/"+preset.replace("\\","/"))
    config=read(path)
    for match in re.finditer(r'^\s*shader\d+\s*=\s*"?([^"\r\n]+)"?',config,re.M):
        file=posixpath.normpath(posixpath.join(posixpath.dirname(path),match[1].strip().replace("\\","/")))
        if file in done:continue
        done.add(file);source=includes(file);stages={1:[],2:[]};active=0
        for line in source.splitlines():
            if line.strip().startswith("#pragma"):
                if re.match(r"\s*#pragma\s+stage\s+vertex",line):active=1
                elif re.match(r"\s*#pragma\s+stage\s+fragment",line):active=2
                continue
            if active in (0,1):stages[1].append(line)
            if active in (0,2):stages[2].append(line)
        stem=file.removeprefix("data/shaders/").replace("/","__")
        for stage,extension in [(1,"vert"),(2,"frag")]:
            target=out/(stem+"."+extension)
            target.write_text("\n".join(stages[stage])+"\n",encoding="utf-8")
            entries.append({"file":target.name,"source":file,"stage":extension})
(out/"manifest.json").write_text(json.dumps(entries,indent=2),encoding="utf-8")
print("Extracted",len(done),"distinct shader passes from",len(presets),"preset entries.")
