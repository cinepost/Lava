#
# This document is under CC-3.0 Attribution-Share Alike 3.0
#       http://creativecommons.org/licenses/by-sa/3.0/
#  Attribution:  There is no requirement to attribute the author.

# Create a simple SOHO scene and traverse the objects
import soho, sys

# Evaluate the 'camera' parameter as a string.
# If the 'camera' parameter # doesn't exist, use ['/obj/cam1'].
# SOHO always returns lists of values.
camera = soho.getDefaultedString('camera', ['/obj/cam1'])[0]

# Evaluate an intrinsic parameter (see HDK_SOHO_API::evaluate())
# The 'state:time' parameter evaluates the time from the ROP.
evaltime = soho.getDefaultedFloat('state:time', [0.0])[0]

# Initialize SOHO with the camera.
if not soho.initialize(evaltime, camera):
    soho.error('Unable to initialize rendering module with camera: %s' %
                repr(camera))

# Now, add objects to our scene
#   addObjects(time, geometry, light, fog, use_display_flags)

objlist = soho.getDefaultedString('objlist', ["*"])[0]
lightlist = soho.getDefaultedString('lightlist', ["*"])[0]

soho.addObjects(evaltime, objlist, lightlist, "*", True)

# Before we can evaluate the scene from SOHO, we need to lock the object lists.
soho.lockObjects(evaltime)

# Now, traverse all the objects
def outputObjects(fp, prefix, list):
    fp.write('%s = {' % prefix)
    for obj in list:
        fp.write('"%s",' % obj.getName())
    fp.write('}\n')
    fp.flush()

fp = sys.stdout
#outputObjects(fp, 'Geometry', soho.objectList('objlist:instance'))
#outputObjects(fp, 'Light', soho.objectList('objlist:light'))
#outputObjects(fp, 'Fog', soho.objectList('objlist:fog'))

fp.write("1")
fp.flush()