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
# NAME: LSDhooks.py ( Python )
#
# COMMENTS:  LSD call-back hooks.  This helps avoiding monkey patching
#

'''
    This file will import methods from a LSDuserhooks.py file.

    The LSDuserhooks.py file can contain various functions which are called
    during LSD generation.  Look through the LSD*.py files for LSDhooks.call().

    To prevent import recursion, the LSDuserhooks file should be careful how it 
    performs importing of other modules.  For example, if you write some code
    which needs to import part of SOHO (i.e. LSDsettings.py) which imports
    LSDhooks, you can get bad recursion.  You might try something like:

        _MODULES = [ 'outputHooks',
                     'cameraHooks',
                     'instanceHooks' ]
        for hook in _MODULES:
            exec('_%s = None' % hook)

        _magicImport = """
        global _%s
        import %s
        _%s = %s"""

        def _importHookModules():
            for hook in _MODULES:
                exec(_magicImport % ( hook, hook, hook, hook ))

        def pre_lockObjects(parmlist, objparms, now, camera):
            _importHookModules()

    The LSDuserhooks.py file should be installed in
            $HOUDINI_PATH/soho/LSDuserhooks.py
'''


from LSDapi import *

try:
    from LSDuserhooks import *
except ImportError:
    def call(name='', *args, **kwargs):
        return False
except Exception as e:
    error = e
    def call(name='', *args, **kwargs):
        cmd_comment('Error: %s' % str(error))
        return False
