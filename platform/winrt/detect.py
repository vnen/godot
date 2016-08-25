import os

import sys
import string


def is_active():
	return True

def get_name():
		return "WinRT"

def can_build():
	if (os.name=="nt"):
		#building natively on windows!
		if (os.getenv("VSINSTALLDIR")):
			return True			
	return False

def get_opts():
	return []

def get_flags():

	return [
	('tools', 'no'),
	('opus', 'no'),
	('freetype', 'no'),
	('openssl', 'no'),
	]


def configure(env):

	angle_root = em_path=os.environ["ANGLE_ROOT_PATH"] + '/'

	env.Append(CPPPATH=['#platform/winrt','#drivers/windows'])

	arch = ""

	if os.getenv('PLATFORM') == "ARM":

		# compiler commandline
		# debug:   /Yu"pch.h" /MP /GS     /analyze- /W3 /wd"4453" /wd"28204"     /Zc:wchar_t /I"C:\Users\ariel\Documents\Visual Studio 2013\Projects\App2\App2\App2.WindowsPhone\" /I"Generated Files\" /I"ARM\Debug\"   /I"C:\Users\ariel\Documents\Visual Studio 2013\Projects\App2\App2\App2.Shared\" /ZW:nostdlib /Zi /Gm- /Od /sdl /Fd"ARM\Debug\vc120.pdb"   /fp:precise /D "PSAPI_VERSION=2" /D "WINAPI_FAMILY=WINAPI_FAMILY_PHONE_APP" /D "_UITHREADCTXT_SUPPORT=0" /D "_UNICODE" /D "UNICODE" /D "_DEBUG" /errorReport:prompt /WX- /Zc:forScope /RTC1 /ZW /Gd /Oy- /MDd    /Fa"ARM\Debug\"   /EHsc /nologo /Fo"ARM\Debug\"   /Fp"ARM\Debug\App2.WindowsPhone.pch"
		# release: /Yu"pch.h" /MP /GS /GL /analyze- /W3 /wd"4453" /wd"28204" /Gy /Zc:wchar_t /I"C:\Users\ariel\Documents\Visual Studio 2013\Projects\App2\App2\App2.WindowsPhone\" /I"Generated Files\" /I"ARM\Release\" /I"C:\Users\ariel\Documents\Visual Studio 2013\Projects\App2\App2\App2.Shared\" /ZW:nostdlib /Zi /Gm- /O2 /sdl /Fd"ARM\Release\vc120.pdb" /fp:precise /D "PSAPI_VERSION=2" /D "WINAPI_FAMILY=WINAPI_FAMILY_PHONE_APP" /D "_UITHREADCTXT_SUPPORT=0" /D "_UNICODE" /D "UNICODE"             /errorReport:prompt /WX- /Zc:forScope       /ZW /Gd /Oy- /Oi /MD /Fa"ARM\Release\" /EHsc /nologo /Fo"ARM\Release\" /Fp"ARM\Release\App2.WindowsPhone.pch"

		# linker commandline
		# debug:   /OUT:"C:\Users\ariel\Documents\Visual Studio 2013\Projects\App2\ARM\Debug\App2.WindowsPhone\App2.WindowsPhone.exe"   /MANIFEST:NO       /NXCOMPAT /PDB:"C:\Users\ariel\Documents\Visual Studio 2013\Projects\App2\ARM\Debug\App2.WindowsPhone\App2.WindowsPhone.pdb"   /DYNAMICBASE "WindowsPhoneCore.lib" "RuntimeObject.lib" "PhoneAppModelHost.lib" /DEBUG /MACHINE:ARM /NODEFAULTLIB:"kernel32.lib" /NODEFAULTLIB:"ole32.lib" /WINMD /APPCONTAINER /INCREMENTAL /PGD:"C:\Users\ariel\Documents\Visual Studio 2013\Projects\App2\ARM\Debug\App2.WindowsPhone\App2.WindowsPhone.pgd"   /WINMDFILE:"C:\Users\ariel\Documents\Visual Studio 2013\Projects\App2\ARM\Debug\App2.WindowsPhone\App2.winmd"   /SUBSYSTEM:WINDOWS /MANIFESTUAC:NO /ManifestFile:"ARM\Debug\App2.WindowsPhone.exe.intermediate.manifest"            /ERRORREPORT:PROMPT /NOLOGO /TLBID:1
		# release: /OUT:"C:\Users\ariel\Documents\Visual Studio 2013\Projects\App2\ARM\Release\App2.WindowsPhone\App2.WindowsPhone.exe" /MANIFEST:NO /LTCG /NXCOMPAT /PDB:"C:\Users\ariel\Documents\Visual Studio 2013\Projects\App2\ARM\Release\App2.WindowsPhone\App2.WindowsPhone.pdb" /DYNAMICBASE "WindowsPhoneCore.lib" "RuntimeObject.lib" "PhoneAppModelHost.lib" /DEBUG /MACHINE:ARM /NODEFAULTLIB:"kernel32.lib" /NODEFAULTLIB:"ole32.lib" /WINMD /APPCONTAINER /OPT:REF     /PGD:"C:\Users\ariel\Documents\Visual Studio 2013\Projects\App2\ARM\Release\App2.WindowsPhone\App2.WindowsPhone.pgd" /WINMDFILE:"C:\Users\ariel\Documents\Visual Studio 2013\Projects\App2\ARM\Release\App2.WindowsPhone\App2.winmd" /SUBSYSTEM:WINDOWS /MANIFESTUAC:NO /ManifestFile:"ARM\Release\App2.WindowsPhone.exe.intermediate.manifest" /OPT:ICF /ERRORREPORT:PROMPT /NOLOGO /TLBID:1

		arch = "arm"

		env.Append(LINKFLAGS=['/INCREMENTAL:NO', '/MANIFEST', '/NXCOMPAT', '/DYNAMICBASE', "WindowsPhoneCore.lib", "RuntimeObject.lib", "PhoneAppModelHost.lib", "/DEBUG", "/MACHINE:ARM", '/NODEFAULTLIB:"kernel32.lib"', '/NODEFAULTLIB:"ole32.lib"', '/WINMD', '/APPCONTAINER', '/MANIFESTUAC:NO', '/ERRORREPORT:PROMPT', '/NOLOGO', '/TLBID:1'])
		env.Append(LIBPATH=['#platform/winrt/ARM/lib'])

		env.Append(CCFLAGS=string.split('/FS /MP /GS /wd"4453" /wd"28204" /analyze- /Zc:wchar_t /Zi /Gm- /Od /fp:precise /fp:precise /D "PSAPI_VERSION=2" /D "WINAPI_FAMILY=WINAPI_FAMILY_PHONE_APP" /DWINDOWSPHONE_ENABLED /D "_UITHREADCTXT_SUPPORT=0" /D "_UNICODE" /D "UNICODE" /errorReport:prompt /WX- /Zc:forScope /Gd /Oy- /Oi /Gd /EHsc /nologo'))
		env.Append(CXXFLAGS=string.split('/ZW /FS'))
		#env.Append(CPPFLAGS=['/FU', 'C:/Program Files (x86)/Windows Phone Kits/8.1/References/CommonConfiguration/Neutral/Windows.winmd'])
		#env.Append(CPPFLAGS=['/FU', 'platform.winmd'])

		if (env["target"]=="release"):

			env.Append(CCFLAGS=['/O2'])
			env.Append(LINKFLAGS=['/SUBSYSTEM:WINDOWS'])

		elif (env["target"]=="test"):

			env.Append(CCFLAGS=['/O2','/DDEBUG_ENABLED','/DD3D_DEBUG_INFO'])
			env.Append(LINKFLAGS=['/SUBSYSTEM:CONSOLE'])

		elif (env["target"]=="debug"):

			env.Append(CCFLAGS=['/Zi','/DDEBUG_ENABLED','/DD3D_DEBUG_INFO','/RTC1'])
			env.Append(LINKFLAGS=['/SUBSYSTEM:CONSOLE'])
			env.Append(LINKFLAGS=['/DEBUG'])

		elif (env["target"]=="profile"):

			env.Append(CCFLAGS=['-g','-pg'])
			env.Append(LINKFLAGS=['-pg'])


		env['ENV'] = os.environ;
		# fix environment for windows phone 8.1
		env['ENV']['WINDOWSPHONEKITDIR'] = env['ENV']['WINDOWSPHONEKITDIR'].replace("8.0", "8.1") # wtf
		env['ENV']['INCLUDE'] = env['ENV']['INCLUDE'].replace("8.0", "8.1")
		env['ENV']['LIB'] = env['ENV']['LIB'].replace("8.0", "8.1")
		env['ENV']['PATH'] = env['ENV']['PATH'].replace("8.0", "8.1")
		env['ENV']['LIBPATH'] = env['ENV']['LIBPATH'].replace("8.0\\Windows Metadata", "8.1\\References\\CommonConfiguration\\Neutral")

	else:

		env.Append(LINKFLAGS=['/MANIFEST:NO', '/NXCOMPAT', '/DYNAMICBASE', '/WINMD', '/APPCONTAINER', '/SAFESEH', '/ERRORREPORT:PROMPT', '/NOLOGO', '/TLBID:1', '/NODEFAULTLIB:"kernel32.lib"', '/NODEFAULTLIB:"ole32.lib"'])
		env.Append(CPPFLAGS=['/D','__WRL_NO_DEFAULT_LIB__'])
		env.Append(CPPFLAGS=['/FU', os.environ['VCINSTALLDIR'] + 'lib/store/references/platform.winmd'])
		env.Append(LIBPATH=['#platform/winrt/x64/lib'])
		env.Append(LIBPATH=[os.environ['VCINSTALLDIR'] + 'lib/store'])
		env.Append(LIBPATH=['C:/Program Files (x86)/Windows Phone Kits/8.1/lib/x86'])
		
		if (env["bits"] == "32"):
			arch = "x86"
			env.Append(CPPFLAGS=['/DPNG_ABORT=abort'])
			env.Append(LINKFLAGS=['/MACHINE:X86'])
		else:
			env["bits"] = "64"
			arch = "x64"
			env.Append(LINKFLAGS=['/MACHINE:X64'])
		


		if (env["target"]=="release"):

			env.Append(CPPFLAGS=['/O2', '/GL'])
			env.Append(CPPFLAGS=['/MD'])
			env.Append(LINKFLAGS=['/SUBSYSTEM:WINDOWS', '/LTCG'])

		elif (env["target"]=="test"):

			env.Append(CCFLAGS=['/O2','/DDEBUG_ENABLED','/DD3D_DEBUG_INFO'])
			env.Append(LINKFLAGS=['/SUBSYSTEM:CONSOLE'])

		elif (env["target"]=="debug"):

			env.Append(CCFLAGS=['/Zi','/DDEBUG_ENABLED','/DDEBUG_MEMORY_ENABLED'])
			env.Append(CPPFLAGS=['/MDd'])
			env.Append(LINKFLAGS=['/SUBSYSTEM:CONSOLE'])
			env.Append(LINKFLAGS=['/DEBUG'])

		elif (env["target"]=="profile"):

			env.Append(CCFLAGS=['-g','-pg'])
			env.Append(LINKFLAGS=['-pg'])


		env.Append(CCFLAGS=string.split('/FS /MP /GS /wd"4453" /wd"28204" /wd"4291" /Zc:wchar_t /Gm- /fp:precise /D "_UNICODE" /D "UNICODE" /D "WINAPI_FAMILY=WINAPI_FAMILY_APP" /errorReport:prompt /WX- /Zc:forScope /Gd /EHsc /nologo'))
		env.Append(CXXFLAGS=string.split('/ZW /FS'))
		env.Append(CCFLAGS=['/AI', os.environ['VCINSTALLDIR']+'\\vcpackages', '/AI', os.environ['WINDOWSSDKDIR']+'\\References\\CommonConfiguration\\Neutral'])
		env.Append(CCFLAGS=['/DWINAPI_FAMILY=WINAPI_FAMILY_APP']) #'/D_WIN32_WINNT=0x0603', '/DNTDDI_VERSION=0x06030000'

		env['ENV'] = os.environ;


	env["PROGSUFFIX"]="."+arch+env["PROGSUFFIX"]
	env["OBJSUFFIX"]="."+arch+env["OBJSUFFIX"]
	env["LIBSUFFIX"]="."+arch+env["LIBSUFFIX"]


	#env.Append(CCFLAGS=['/Gd','/GR','/nologo', '/EHsc'])
	#env.Append(CXXFLAGS=['/TP', '/ZW'])
	#env.Append(CPPFLAGS=['/DMSVC', '/GR', ])
	##env.Append(CCFLAGS=['/I'+os.getenv("WindowsSdkDir")+"/Include"])
	env.Append(CCFLAGS=['/DWINRT_ENABLED'])
	env.Append(CCFLAGS=['/DWINDOWS_ENABLED'])
	#env.Append(CCFLAGS=['/DWIN32'])
	env.Append(CCFLAGS=['/DTYPED_METHOD_BIND'])

	env.Append(CCFLAGS=['/DGLES2_ENABLED','/DGL_GLEXT_PROTOTYPES','/DEGL_EGLEXT_PROTOTYPES','/DANGLE_ENABLED'])
	#env.Append(CCFLAGS=['/DGLES1_ENABLED'])

	LIBS=[
		#'winmm',
		#'libEGL',
		#'libGLESv2',
		#'libANGLE',
		'xaudio2',
		#'WindowsPhoneCore',
		#'kernel32',
		#'ole32',
		'WindowsApp',
		'mincore',
		#'C:/Program Files (x86)/Windows Kits/10/Lib/10.0.14393.0/um/x86/kernel32',
		#'C:/Program Files (x86)/Windows Kits/10/Lib/10.0.14393.0/um/x86/ole32',
		#'kernel32','ole32','user32', 'advapi32'
		]
	env.Append(LINKFLAGS=[p+".lib" for p in LIBS])
	
	# Incremental linking fix
	env['BUILDERS']['ProgramOriginal'] = env['BUILDERS']['Program']
	env['BUILDERS']['Program'] = precious_program

	import methods
	env.Append( BUILDERS = { 'GLSL120' : env.Builder(action = methods.build_legacygl_headers, suffix = 'glsl.h',src_suffix = '.glsl') } )
	env.Append( BUILDERS = { 'GLSL' : env.Builder(action = methods.build_glsl_headers, suffix = 'glsl.h',src_suffix = '.glsl') } )
	env.Append( BUILDERS = { 'HLSL9' : env.Builder(action = methods.build_hlsl_dx9_headers, suffix = 'hlsl.h',src_suffix = '.hlsl') } )
	env.Append( BUILDERS = { 'GLSL120GLES' : env.Builder(action = methods.build_gles2_headers, suffix = 'glsl.h',src_suffix = '.glsl') } )


#/c/Program Files (x86)/Windows Phone Kits/8.1/lib/ARM/WindowsPhoneCore.lib


def precious_program(env, program, sources, **args):
	program = env.ProgramOriginal(program, sources, **args)
	env.Precious(program)
	return program
