/*
MIT License

Copyright(c) 2016 Trend Micro Incorporated

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files(the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once
#include <windows.h>
#include <winsock2.h>
#include <Iphlpapi.h>
#include <vector>
#include <cstring>

#include <protocols/layer2/common.hpp>

namespace windows {

	const uint8_t if_guid_length = 38;
	const uint8_t if_guid_libpcap_length = 50;
	const uint8_t if_mac_length = 6;

	struct netif 
	{
		/*
			if_name: have the format \Device\NPF_{B26DF21F-0A49-4D44-A98E-7E6242968209}.
		*/
		static bool get_network_adapter(const char* if_guid_libpcap, uint64_t& mac)
		{
			if (if_guid_libpcap == 0)
				return false;

			if(std::strlen(if_guid_libpcap) != if_guid_libpcap_length)
				return false;
			
			mac = 0;
			PIP_ADAPTER_ADDRESSES addresses = 0;
			ULONG out_buf_len = 0;
			if(GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, 0, addresses, &out_buf_len) != ERROR_BUFFER_OVERFLOW)
				return false;

			std::vector<byte> out_buffer;
			out_buffer.resize(out_buf_len);
			addresses = static_cast<PIP_ADAPTER_ADDRESSES>((void*)&out_buffer[0]);

			if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, 0, addresses, &out_buf_len) != NO_ERROR)
				return false;

			//to scape "\Device\NPF_" in \Device\NPF_{B26DF21F-0A49-4D44-A98E-7E6242968209}
			const char* if_guid_libpcap_tmp = if_guid_libpcap;			
			if_guid_libpcap_tmp+= if_guid_libpcap_length - if_guid_length; 

			for (PIP_ADAPTER_ADDRESSES address = addresses; address; address = address->Next)
			{
				if (address->PhysicalAddressLength != if_mac_length || address->AdapterName == 0)
					continue;

				if(std::strlen(address->AdapterName) != if_guid_length)
					continue;				
				
				if(std::strncmp(if_guid_libpcap_tmp, address->AdapterName, if_guid_length) != 0)
					continue;
				
				mac = layer2::mac_address::to_uint64_t(address->PhysicalAddress);
				return true;							
			}

			return false;
		}
	};

	struct system 
	{
		static void sleep(int ms) { Sleep(ms); }
	};

	struct fs 
	{
		//delay: delay between files, in milliseconds
		template <class Taction>
		static bool dir_files(const char* path, Taction& action, int delay = 0)
		{
			WIN32_FIND_DATAA file;
			HANDLE hFind = FindFirstFileA(path, &file);

			if (hFind == INVALID_HANDLE_VALUE)
				return false;

			if (file.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE)
			{
				system::sleep(delay);
				action.do_action(path);
				FindClose(hFind);
				return true;
			}

			if (file.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) 
			{
				FindClose(hFind);
				std::string dir(path);
				dir.append("\\*.*");
				hFind = FindFirstFileA(dir.c_str(), &file);

				while (FindNextFileA(hFind, &file))
				{
					if (file.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE)
					{
						std::string full_path(path);
						full_path.append("\\");
						full_path.append(file.cFileName);
						system::sleep(delay);
						action.do_action(full_path.c_str());
					}
				}
			}

			FindClose(hFind);
			return false;
		}

		template <class Taction, class Tsignal>
		static bool dir_files_recursive(const char* path, Taction& action, Tsignal* sig, int delay = 0)
		{
			if (!sig->is_running())
				return true;

			WIN32_FIND_DATAA file;
			HANDLE hFind = FindFirstFileA(path, &file);

			if (hFind == INVALID_HANDLE_VALUE)
				return false;

			//if ((file.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE)
				  //|| (file.dwFileAttributes & FILE_ATTRIBUTE_NORMAL))
			if (!(file.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				if (!sig->is_running())
					return true;

				system::sleep(delay);
				action.do_action(path);
				FindClose(hFind);
				return true;
			}

			if (file.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				FindClose(hFind);
				std::string dir(path);
				dir.append("\\*.*");
				hFind = FindFirstFileA(dir.c_str(), &file);

				while (FindNextFileA(hFind, &file))
				{
					if (!sig->is_running())
						return true;

					if (file.cFileName[0] == '.' && file.cFileName[1] == 0)
						continue;

					if (file.cFileName[0] == '.'
						&& file.cFileName[1] == '.'
						&& file.cFileName[2] == 0)
						continue;

					std::string full_path(path);
					full_path.append("\\");
					full_path.append(file.cFileName);

					//FILE_ATTRIBUTE_NORMAL: no attribute was set in the file
					//if ((file.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE)
						//|| (file.dwFileAttributes & FILE_ATTRIBUTE_NORMAL))
					if (!(file.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
					{
						if (!sig->is_running())
							return true;

						system::sleep(delay);
						action.do_action(full_path.c_str());
					}
										
					if (file.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
						dir_files_recursive(full_path.c_str(), action, sig);
				}
			}

			FindClose(hFind);
			return false;
		}		
	};
}