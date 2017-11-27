#!/usr/bin/python
import sys
import os
import subprocess
import argparse

#Script for performance testing for SoCRocket
#TODO automatisches downloaden und integrieren vom flamegraph repository
#TODO dass perf auch installiert ist
# vgl. core/waf/cmake.py
#TODO gephi tool oder xml vorlage --> not possible
#TODO perf.data sodass user damit arbeiten kann, ohne sudo
#TODO via python. "chrown -R {}"

def flame( name, rootuser):
    with open(name + ".perf", "w") as flame_perf_file,open(name + ".perf.folded", "w") as flame_perf_folded_file,open(name + "_flamegraph.svg", "w") as flame_image_file:
        print "generate perf report with c++filt"
        if rootuser:
            fg_p1 = subprocess.Popen([#"sudo", #needed to write perf.data
                "perf",
                "script"], #saves perf results in perf.data
                 stdout=subprocess.PIPE)
        else:
            fg_p1 = subprocess.Popen(["sudo", #needed to write perf.data
                "perf",
                "script"], #saves perf results in perf.data
                 stdout=subprocess.PIPE)

        fg_p2 = subprocess.Popen(["c++filt"],
            stdin=fg_p1.stdout,
            stdout=flame_perf_file)
        fg_p2.communicate()[0]
        fg_p2.wait()
        if (0==os.stat(flame_perf_file.name).st_size):
            fg_p1 = subprocess.Popen(["sudo", #needed to write perf.data
                "perf",
                "script"], #saves perf results in perf.data
                 stdout=subprocess.PIPE)
            fg_p2 = subprocess.Popen(["c++filt"],
                stdin=fg_p1.stdout,
                stdout=flame_perf_file)
        print "fold report data"
        print wd+flame_perf_file.name
        fg_p3 = subprocess.Popen([wd+"build/.conf_check_deps/src/FlameGraph-1.0/stackcollapse-perf.pl", wd+flame_perf_file.name], stdout=flame_perf_folded_file)
        fg_p3.wait()
        print "generate image"
        fg_p4 = subprocess.Popen([wd+"build/.conf_check_deps/src/FlameGraph-1.0/flamegraph.pl", wd+flame_perf_folded_file.name], stdout=flame_image_file)
        fg_p4.wait()

def dot( name, rootuser):
    #perf script | c++filt | gprof2dot.py -f perf | dot -Tpng -o output.png
    cg_image_file= name+"_callgraph.png"
    if rootuser:
        cg_p1 = subprocess.Popen([#"sudo",
            "perf",
            "script" #saves perf results in perf.data
            ], stdout=subprocess.PIPE)
    else: #TODO or cg_image_file.size=0
        cg_p1 = subprocess.Popen(["sudo",
            "perf",
            "script" #saves perf results in perf.data
            ], stdout=subprocess.PIPE)
    cg_p2 = subprocess.Popen(["c++filt"],
        stdin=cg_p1.stdout,
        stdout=subprocess.PIPE)
    if (0==os.stat(cg_image_file).st_size):
        cg_p1 = subprocess.Popen(["sudo",
            "perf",
            "script" #saves perf results in perf.data
            ], stdout=subprocess.PIPE)
        cg_p2 = subprocess.Popen(["c++filt"],
            stdin=cg_p1.stdout,
            stdout=subprocess.PIPE)
    print "generating dot graph of "+name
    cg_p3 = subprocess.Popen([ #subprocess.check_output()
        "gprof2dot", # creates a dot graph
        "-f", "perf", #format perf
        "--strip", #strip funktction parameters
        "--wrap" #wrap function names
    ], stdin=cg_p2.stdout, stdout=subprocess.PIPE)
    cg_p4 = subprocess.check_output(("dot -Tpng -o "+ cg_image_file).split(), stdin=cg_p3.stdout)

def txt(name,rootuser):
    #perf report --sort comm,dso | c++filt >> output.txt
    print "Generating txt"
    with open(name+"_report.txt","w") as cg_report_file:
        if rootuser:
            txt_p1 = subprocess.Popen([#"sudo",
            "perf",
            "report",
            "--sort",
            "comm,dso"], stdout=subprocess.PIPE)
        else:
            txt_p1 = subprocess.Popen(["sudo",
            "perf",
            "report",
            "--sort",
            "comm,dso"], stdout=subprocess.PIPE)
        txt_p2 = subprocess.Popen("c++filt", stdin=txt_p1.stdout, stdout=cg_report_file)
        txt_p2.wait();
        if (0==os.stat(cg_report_file.name).st_size):
            txt_p1 = subprocess.Popen(["sudo",
            "perf",
            "report",
            "--sort",
            "comm,dso"], stdout=subprocess.PIPE)
            txt_p2 = subprocess.Popen("c++filt", stdin=txt_p1.stdout, stdout=cg_report_file)
            txt_p2.wait();


print "SocRocket Performance"

parser = argparse.ArgumentParser()
parser.add_argument('filename')
parser.add_argument("--asroot", help="run this script as root", action="store_true")
parser.add_argument("--asuser", help="run this script as user", action="store_true")
args = parser.parse_args()

#get path from first argument
origin = args.filename
while (None == origin):
    origin = raw_input("Path? ")
origin_norm= os.path.realpath(origin)
#get filename from path
name = os.path.basename(origin_norm)
#update current working directory
wd=os.getcwd()+os.sep
#
asroot = False
if args.asroot:
    asroot = True
print "collecting performance data of %s" %(origin_norm)
if asroot:
    perf_record = "perf record --call-graph dwarf -F 99 %s" %(origin_norm)
else:
    perf_record = "sudo perf record --call-graph dwarf -F 99 %s" %(origin_norm)
#kernel.perf_event_paranoid
print perf_record
fg_p0 = subprocess.Popen(perf_record.split())
fg_p0.wait()
#repeat
#get user permission
cwd=os.getcwd()
print cwd
if os.stat('perf.data').st_size>0:
    #post_processing
    flame(name,asroot)
    dot(name,asroot)
    txt(name,asroot)
    print "deleting system files"
    os.remove("perf.data")
    os.remove(name+".perf")
    os.remove(name+".perf.folded")
else:
    print "No samples recorded"
