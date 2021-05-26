class VopAdapterOutputsCountMismatchError(Exception):
    def __init__(self, adapter, message=None):
        self.adapter = adapter
        self.message = message or ('Vop node outputs count not supported by registered adapter of type "%s"' % adapter.vopTypeName())
        super(VopAdapterOutputsCountMismatchError, self).__init__(self.message)
