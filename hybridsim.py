import ctypes
from ctypes import byref
from ctypes import c_ulonglong

lib = ctypes.cdll.LoadLibrary('./libhybridsim.so')

class HybridSim(object):
	def __init__(self, sys_id, ini):
		# Get an instance of the HybridSim_C_Wrapper
		self.hs = lib.HybridSim_C_getMemorySystemInstance(sys_id, ini)

		# External callbacks for Python interface.
		self.read_cb = None
		self.write_cb = None
		self.notify_cb = None

		# Define the callback functions that will receive callbacks from the C interface.
		def c_read_cb(sysID, addr, cycle):
			#print 'Received C read callback in Python: (%d, %d, %d)'%(sysID, addr, cycle)
			if self.read_cb != None:
				self.read_cb(sysID, addr, cycle)
		def c_write_cb(sysID, addr, cycle):
			#print 'Received C write callback in Python: (%d, %d, %d)'%(sysID, addr, cycle)
			if self.write_cb != None:
				self.write_cb(sysID, addr, cycle)
		def c_notify_cb(operation, addr, cycle):
			#print 'Received C write callback in Python: (%d, %d, %d)'%(sysID, addr, cycle)
			if self.notify_cb != None:
				self.notify_cb(operation, addr, cycle)

		# Save the callbacks in the class
		# This is essential so the Python garbage collector doesn't nuke these functions
		# and cause weird internal Python errors at runtime.
		self.c_read_cb = c_read_cb
		self.c_write_cb = c_write_cb
		self.c_notify_cb = c_notify_cb

		# Define the ctypes wrapper for the callbacks.
		CBFUNC = ctypes.CFUNCTYPE(None, ctypes.c_uint, ctypes.c_ulonglong, ctypes.c_ulonglong)

		# Also save all CBFUNC objects, as the ctypes documentation says to do.
		self.CBFUNC_read = CBFUNC(self.c_read_cb)
		self.CBFUNC_write = CBFUNC(self.c_write_cb)
		self.CBFUNC_notify = CBFUNC(self.c_notify_cb)

		# Register the callbacks.
		lib.HybridSim_C_RegisterCallbacks(self.hs, self.CBFUNC_read, self.CBFUNC_write)
		lib.HybridSim_C_RegisterNotifyCallback(self.hs, self.CBFUNC_notify)
		

	def RegisterCallbacks(self, read_cb, write_cb):
		self.read_cb = read_cb
		self.write_cb = write_cb

	def addTransaction(self, isWrite, addr):
		return lib.HybridSim_C_addTransaction(self.hs, isWrite, c_ulonglong(addr))

	def WillAcceptTransaction(self):
		return lib.HybridSim_C_WillAcceptTransaction(self.hs)

	def update(self):
		lib.HybridSim_C_update(self.hs)

	def mmio(self, operation, address):
		lib.HybridSim_C_mmio(self.hs, operation, address)

	def syncAll(self):
		lib.HybridSim_C_syncAll(self.hs)

	def reportPower(self):
		lib.HybridSim_C_reportPower(self.hs)

	def printLogfile(self):
		lib.HybridSim_C_printLogfile(self.hs)

	def RegisterNotifyCallback(self, notify_cb):
		self.notify_cb = notify_cb

	def ConfigureNotify(self, operation, enable):
		lib.HybridSim_C_ConfigureNotify(self.hs, operation, enable)

	def query(self, operation, input1, input2):
		output1 = c_ulonglong()
		output2 = c_ulonglong()
		lib.HybridSim_C_query(self.hs, operation, input1, input2, byref(output1), byref(output2))
		return (output1.value, output2.value)
		

def read_cb(sysID, addr, cycle):
	print 'cycle %d: read callback from sysID %d for addr = %d'%(cycle, sysID, addr)
def write_cb(sysID, addr, cycle):
	print 'cycle %d: write callback from sysID %d for addr = %d'%(cycle, sysID, addr)
def notify_cb(operation, addr, cycle):
	print 'cycle %d: notify callback from operation %d for addr = %d'%(cycle, operation, addr)
		
def main():
	hs = HybridSim(0, '')
	hs.RegisterCallbacks(read_cb, write_cb)
	hs.RegisterNotifyCallback(notify_cb)
	hs.ConfigureNotify(0, True)

	hs.addTransaction(1, 0)
	hs.addTransaction(1, 8)
	hs.addTransaction(1, 16)
	hs.addTransaction(1, 24)

	for i in range(100000):
		hs.update()

	hs.WillAcceptTransaction()
	hs.reportPower()
	hs.mmio(0,0)
	print hs.query(0, 1, 2)
	hs.syncAll()

	hs.printLogfile()


if __name__ == '__main__':
	main()

