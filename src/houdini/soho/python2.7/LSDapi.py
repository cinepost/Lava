#
# PROPRIETARY INFORMATION.  This software is proprietary to
# Side Effects Software Inc., and is not to be reproduced,
# transmitted, or disclosed in any way without written permission.
#
# Produced by:
#       Side Effects Software Inc
#       123 Front Street West, Suite 1401
#       Toronto, Ontario
#       Canada   M5J 2M2
#       416-504-9876
#
# NAME:         LSDapi.py ( Python )
#
# COMMENTS:     Python utilities for LSD interface
#

import sys, os
import soho
from soho import Precision

#########################################################
#
#  Utility functions
#
#########################################################

ForceEmbedVex = False

def cmd_quote(str):
    return '"' + str.replace('"', '\"') + '"'

def cmd_formatReal(v):
    return "%.*g" % (Precision, v)

#########################################################
#
#  Start of the LSD API
#
#########################################################

def cmd_comment(msg):
    if msg != None:
        print "#", msg
    else:
        print

def cmd_setenv(name, value):
    soho.indent()
    print 'setenv', name, '=', cmd_quote(value)

def cmd_version(ver):
    soho.indent()
    print 'cmd_version VER%s' % ver

def cmd_hscript(cmd):
    # The LSD language is a subset of the hscript language.  This command lets
    # you inject hscript commands into the LSD stream
    print cmd

def cmd_defaults(file):
    soho.indent()
    print 'cmd_defaults', repr(file)

def cmd_config(file):
    soho.indent()
    print 'cmd_config', repr(file)

def cmd_loadotl(otls, from_text_block=False):
    cmd = 'cmd_loadotl '
    if from_text_block:
        cmd += '-t '
    for l in otls:
        soho.indent()
        soho.printArray(cmd, [l], '\n')

def cmd_pathmap(src, target):
    src = src.strip()
    target = target.strip()
    if src and target:
        soho.indent()
        soho.printArray('pathmap -a ', [src, target], '\n')

def cmd_otprefer(otprefs):
    for i in range(0, len(otprefs), 2):
        soho.indent()
        soho.printArray('otprefer ', [otprefs[i], otprefs[i+1]], '\n')

def cmd_transform(matrix):
    soho.indent()
    soho.printArray('cmd_transform ', matrix, '\n')

def cmd_mtransform(matrix):
    soho.indent()
    soho.printArray('cmd_mtransform ', matrix, '\n')

def cmd_stransform(matrix):
    soho.printArray('cmd_stransform ', matrix, '\n')

def cmd_detail(name, file):
    soho.indent()
    if file.find(' ') >= 0:
        file = '"%s"' % file
    print 'cmd_detail', name, file

def cmd_geometry(file):
    soho.indent()
    print 'cmd_geometry', file

def cmd_start(objecttype):
    soho.indent(1, ' '.join(['cmd_start', objecttype]))

def cmd_end():
    soho.indent(-1, 'cmd_end')

def cmd_delete(type, name):
    print 'cmd_delete', type, name

def cmd_procedural(bounds, proc, shoptype = soho.ShopTypeDefault):
    soho.indent()
    shader = soho.processShader(proc[0], ForceEmbedVex, False, shoptype)
    if len(shader[1]):
        print shader[1]
    if bounds[0] <= bounds[3]:
        soho.printArray('cmd_procedural -m ', bounds[0:3], '')
        soho.printArray(              ' -M ', bounds[3:6], ' '),
    else:
        sys.stdout.write('cmd_procedural ')
    sys.stdout.write(shader[0])
    sys.stdout.write('\n')
    print

def cmd_shop(shop, shaderstring, shoptype = soho.ShopTypeDefault):
    # Declare an old style shader
    shader = soho.processShader(shaderstring, ForceEmbedVex, False, shoptype)
    if len(shader[1]):
        print shader[1]
    soho.indent()
    print 'cmd_shop', shop, shader[0]

def cmd_shader(style, name, orgshader, shoptype = soho.ShopTypeDefault):
    # Processing  the shader may generate VEX code or COP maps and
    # will return the value in the second element of the tuple.  This
    # should be printed before the shader string.
    shader = soho.processShader(orgshader, ForceEmbedVex, False, shoptype)
    if len(shader[1]):
        print shader[1]
    soho.indent()
    print 'cmd_property', style, name, shader[0]

def cmd_textblock(name, value, encoding=None):
    soho.indent()
    if encoding:
        print 'cmd_textblock -e', encoding, name
    else:
        print 'cmd_textblock', name
    print value
    print 'cmd_endtext'

def cmd_erase_textblock(name):
    soho.indent()
    print 'cmd_textblock -x', name

def cmd_stylesheet(name):
    soho.indent()
    print 'cmd_stylesheet', name

def cmd_bundle(bundle, node_paths):
    soho.indent()
    print 'cmd_bundlecreate', bundle
    soho.addBundleDependency(bundle)
    start = 0
    step = 100
    while len(node_paths) > start:
        end = start + step
        print 'cmd_bundleadd', bundle, " ".join(node_paths[start:end])
        start = end

def cmd_property(style, name, value, quoted=True):
    soho.indent()
    soho.printArray('cmd_property %s %s ' % (style, name), value, '\n', quoted)

def cmd_propertyAndParms(style, name, value, parms):
    soho.indent()
    sys.stdout.write('cmd_property %s %s %s' % (style, name, value))
    for parm in parms:
        parm.printValue(' ' + parm.Key + ' ', '')
    sys.stdout.write('\n')

def cmd_propertyV(style, parmlist):
    if style:
        for parm in parmlist:
            soho.indent()
            parm.printValue('cmd_property %s %s ' % (style, parm.Key), '\n')
    else:
        for parm in parmlist:
            soho.indent()
            parm.printValue('cmd_property %s %s '%(parm.Style, parm.Key), '\n')

def cmd_odefprop(name, type, value):
    soho.indent()
    soho.printArray('cmd_odefprop %s %s ' % (name, type), value, '\n')

def cmd_declare(style, type, name, value):
    soho.indent()
    soho.printArray('cmd_declare %s %s %s ' % (style, type, name), value, '\n')

def cmd_time(now):
    soho.indent()
    cmd = 'cmd_time %s' % cmd_formatReal(now)
    soho.indent(1, cmd)

def cmd_commandline(options):
    soho.indent()
    print "cmd_cmdopt", options

def cmd_image(filename, device="", options=""):
    soho.makeFilePathDirsIfEnabled(filename)
    args = []
    if device:
        args.append("-f")
        args.append(device)
    if options:
        options = ' ' + options
    args.append(filename)
    istr = soho.arrayToString('cmd_image ', args, options)
    soho.indent(1, istr, None)

def cmd_photon():
    soho.indent()
    print "cmd_photon"

def cmd_defplane(filename, variable, vextype):
    soho.indent()
    soho.printArray('cmd_defplane ', [filename, variable, vextype], '\n')

def cmd_planeproperty(parm, value):
    soho.indent()
    soho.printArray("cmd_planeproperty %s " % parm, value, "\n")

def cmd_planepropertyV(parmlist):
    for parm in parmlist:
        soho.indent()
        parm.printValue('cmd_planeproperty %s ' % parm.Key, '\n')

def cmd_raytrace():
    soho.indent()
    print 'cmd_raytrace'

def cmd_reset(light=True, obj=True, fog=True):
    options = ''
    if light:
        options += ' -l'
    if obj:
        options += ' -o'
    if fog:
        options += ' -f'
    soho.indent(-1, 'cmd_reset %s' % options)

def cmd_makecubemap(path, suffixes):
    soho.indent()
    # When there are spaces in the path names, the quote protection for this
    # command can be incredibly difficult.
    sys.stdout.write('''python -c 'import os; os.system("isixpack''')
    for s in suffixes:
        sys.stdout.write(' \\"%s%s\\"' % (path, s))
    sys.stdout.write(''' \\"%s\\"")'\n''' % path);
    for s in suffixes:
        soho.indent()
        sys.stdout.write('cmd_unlink -f "%s%s"\n' % ( path, s))

def cmd_prefilter(path, prefilter_path, filter, count, ratio):
    soho.indent()
    # Create a temporary file in the same directory as the original map
    pathsplit = os.path.split(path)
    args = [ '"%s" prefilter' % path,
             'photon_file "%s"' % path,
             'prefiltered_file "%s"' % prefilter_path,
             'count %d' % count,
             'filter %s' % filter,
             'ratio %f' % ratio
           ]
    filterargs = ' '.join(args)
    sys.stdout.write('cmd_pcfilter %s\n' % (filterargs))

def cmd_updateipr(stash=True):
    soho.indent()
    sys.stdout.write('cmd_updateipr');
    if stash:
        sys.stdout.write(' -s')
    sys.stdout.write('\n')

def cmd_if(condition):
    soho.indent(1, 'if %s then' % condition)

def cmd_endif():
    soho.indent(-1, 'endif')

def cmd_quit():
    soho.indent(-1, 'cmd_quit')
