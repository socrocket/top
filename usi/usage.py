
HELP = "SoCRocket - The Space TLM Framework"

CREDITS = "Thomas Schuster, Rolf Meyer, Jan Wagner"

COPYRIGHT = "(c) Copyright 2010-2014 TU-Braunschweig c3e"

LICENSE = "All rights reserved"

USAGE = "%(prog)s [options]"

def help_text(*k, **kw):
    import os
    print HELP
    if not os.environ.has_key('SSH_CONNECTION'):
      import webbrowser
      webbrowser.open('http://socrocket.github.io/')
    elif os.environ.has_key('TMUX'):
      pass
    help(*k, **kw)
def credits_text():
    print CREDITS

def copyright_text():
    print COPYRIGHT

def license_text():
    print LICENSE

