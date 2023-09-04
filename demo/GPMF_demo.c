/*! @file GPMF_demo.c
 *
 *  @brief Demo to extract GPMF from an MP4
 *
 *  @version 2.4.0
 *
 *  (C) Copyright 2017-2020 GoPro Inc (http://gopro.com/).
 *
 *  Licensed under either:
 *  - Apache License, Version 2.0, http://www.apache.org/licenses/LICENSE-2.0
 *  - MIT license, http://opensource.org/licenses/MIT
 *  at your option.
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#include "../GPMF_parser.h"
#include "GPMF_mp4reader.h"
#include "../GPMF_utils.h"

#define	SHOW_VIDEO_FRAMERATE		1
#define	SHOW_PAYLOAD_TIME			1
#define	SHOW_ALL_PAYLOADS			0
#define SHOW_GPMF_STRUCTURE			0
#define	SHOW_PAYLOAD_INDEX			0
#define	SHOW_SCALED_DATA			1
#define	SHOW_THIS_FOUR_CC			STR2FOURCC("ACCL")
#define SHOW_COMPUTED_SAMPLERATES	1



extern void PrintGPMF(GPMF_stream* ms);

void printHelp(char* name)
{
	printf("usage: %s <file_with_GPMF> <optional features>\n", name);
	printf("       -a - %s all payloads\n", SHOW_ALL_PAYLOADS ? "disable" : "show");
	printf("       -g - %s GPMF structure\n", SHOW_GPMF_STRUCTURE ? "disable" : "show");
	printf("       -i - %s index of the payload\n", SHOW_PAYLOAD_INDEX ? "disable" : "show");
	printf("       -s - %s scaled data\n", SHOW_SCALED_DATA ? "disable" : "show");
	printf("       -c - %s computed sample rates\n", SHOW_COMPUTED_SAMPLERATES ? "disable" : "show");
	printf("       -v - %s video framerate\n", SHOW_VIDEO_FRAMERATE ? "disable" : "show");
	printf("       -t - %s time of the payload\n", SHOW_PAYLOAD_TIME ? "disable" : "show");
	printf("       -fWXYZ - show only this fourCC , e.g. -f%c%c%c%c (default) just -f for all\n", PRINTF_4CC(SHOW_THIS_FOUR_CC));
	printf("       -h - this help\n");
	printf("       \n");
	printf("       ver 2.0\n");
}

int main(int argc, char* argv[])
{
	GPMF_ERR ret = GPMF_OK;
	GPMF_stream metadata_stream, * ms = &metadata_stream;
	double metadatalength;
	uint32_t* payload = NULL;
	uint32_t payloadsize = 0;
	size_t payloadres = 0;

	uint32_t show_all_payloads = SHOW_ALL_PAYLOADS;
	uint32_t show_gpmf_structure = SHOW_GPMF_STRUCTURE;
	uint32_t show_payload_index = SHOW_PAYLOAD_INDEX;
	uint32_t show_scaled_data = SHOW_SCALED_DATA;
	uint32_t show_computed_samplerates = SHOW_COMPUTED_SAMPLERATES;
	uint32_t show_video_framerate = SHOW_VIDEO_FRAMERATE;
	uint32_t show_payload_time = SHOW_PAYLOAD_TIME;
	uint32_t show_this_four_cc = SHOW_THIS_FOUR_CC;

	// get file return data
	if (argc < 2)
	{
		printHelp(argv[0]);
		return -1;
	}

#if 1 // Search for GPMF Track
	size_t mp4handle = OpenMP4Source(argv[1], MOV_GPMF_TRAK_TYPE, MOV_GPMF_TRAK_SUBTYPE, 0);
#else // look for a global GPMF payload in the moov header, within 'udta'
	size_t mp4handle = OpenMP4SourceUDTA(argv[1], 0);  //Search for GPMF payload with MP4's udta
#endif
	if (mp4handle == 0)
	{
		printf("error: %s is an invalid MP4/MOV or it has no GPMF data\n\n", argv[1]);

		printHelp(argv[0]);
		return -1;
	}

	for (int i = 2; i < argc; i++)
	{
		if (argv[i][0] == '-') //feature switches
		{
			switch (argv[i][1])
			{
			case 'a': show_all_payloads ^= 1;				break;
			case 'g': show_gpmf_structure ^= 1;				break;
			case 'i': show_payload_index ^= 1;				break;
			case 's': show_scaled_data ^= 1;				break;
			case 'c': show_computed_samplerates ^= 1;		break;
			case 'v': show_video_framerate ^= 1;			break;
			case 't': show_payload_time ^= 1;				break;
			case 'f': show_this_four_cc = STR2FOURCC((&(argv[i][2])));  break;
			case 'h': printHelp(argv[0]);  break;
			}
		}
	}


	metadatalength = GetDuration(mp4handle);
	if (metadatalength > 0.0)
	{
		uint32_t index, payloads = GetNumberPayloads(mp4handle);
		//		printf("found %.2fs of metadata, from %d payloads, within %s\n", metadatalength, payloads, argv[1]);

		uint32_t fr_num, fr_dem;
		uint32_t frames = GetVideoFrameRateAndCount(mp4handle, &fr_num, &fr_dem);

		for (index = 0; index < payloads; index++)
		{
			double in = 0.0, out = 0.0; //times
			payloadsize = GetPayloadSize(mp4handle, index);
			payloadres = GetPayloadResource(mp4handle, payloadres, payloadsize);
			payload = GetPayload(mp4handle, payloadres, index);
			if (payload == NULL)
				goto cleanup;

			ret = GetPayloadTime(mp4handle, index, &in, &out);
			if (ret != GPMF_OK)
				goto cleanup;

			ret = GPMF_Init(ms, payload, payloadsize);
			if (ret != GPMF_OK)
				goto cleanup;


			//HEADERS: 
			//Label,GPS Time,Accuracy,Dimensions,Latitude,Longitude,Altitude,flat_map_speed,Speed (m/s)
			while (GPMF_OK == GPMF_FindNext(ms, STR2FOURCC("STRM"), GPMF_RECURSE_LEVELS|GPMF_TOLERANT))
			{
				if (GPMF_OK != GPMF_FindNext(ms, STR2FOURCC("GPS5"), GPMF_RECURSE_LEVELS|GPMF_TOLERANT))
					continue;
				
				GPMF_stream gpsp_stream, gpsu_stream, gpsf_stream;
				GPMF_CopyState(ms, &gpsp_stream);
				if (GPMF_OK != GPMF_FindPrev(&gpsp_stream, STR2FOURCC("GPSP"), GPMF_CURRENT_LEVEL|GPMF_TOLERANT))
				{
					printf("Wtf whered it go GPSP\n");
					continue;
				}
				uint16_t gpsp; // S scaled by x100
				if (GPMF_OK != GPMF_ScaledData(&gpsp_stream, &gpsp, sizeof(uint16_t), 0, 1, GPMF_TYPE_UNSIGNED_SHORT)) {
					printf("GPSP broke\n");
					continue;
				}

				GPMF_CopyState(ms, &gpsu_stream); // U char[16]
				if (GPMF_OK != GPMF_FindPrev(&gpsu_stream, STR2FOURCC("GPSU"), GPMF_CURRENT_LEVEL|GPMF_TOLERANT))
				{
					printf("Wtf whered it go GPSU\n");
					continue;
				}
				char gpsu[17], gpsu_pretty[24];
				gpsu[16] = 0; // NULL terminate
				gpsu_pretty[23] = 0;
				char *dt_source = GPMF_RawData(&gpsu_stream);
				memcpy(gpsu, dt_source, 16);
				
				// There is probably a more efficient way to do this, but doing it this way helped in debugging a lot.
				sprintf_s(gpsu_pretty, sizeof(gpsu_pretty), "%c%c%c%c-%c%c-%c%cT%c%c:%c%c:%c%c.%c%c%c",
					'2',
					'0', // Only 20xx is supported in this version
					gpsu[0],
					gpsu[1],
					gpsu[2],
					gpsu[3],
					gpsu[4],
					gpsu[5],
					gpsu[6],
					gpsu[7],
					gpsu[8],
					gpsu[9],
					gpsu[10],
					gpsu[11],
					// gpsu[12], // Is a '.'
					gpsu[13],
					gpsu[14],
					gpsu[15]
				);
				struct tm tm_s;
				int ms_remaining; 
				int vars_read = sscanf_s(gpsu_pretty, "%d-%d-%dT%d:%d:%d.%d", &tm_s.tm_year, &tm_s.tm_mon, &tm_s.tm_mday, &tm_s.tm_hour, &tm_s.tm_min, &tm_s.tm_sec, &ms_remaining);
				tm_s.tm_mon -= 1; // Zero based
				tm_s.tm_year -= 1900; // Year is based on delta to 1900

#ifdef _WINDOWS
#define time_struct_parser _mkgmtime
#else
// Probably needs a more concise ifdef to set this
#define time_struct_parser timegm
#endif
				int64_t base_sec =  time_struct_parser(&tm_s);
				double base_time = base_sec + (double)ms_remaining/1000;
				//printf("I think time is %lld : %d: %d-%02d-%02dT%02d:%02d:%02d | %s\n",base_sec, vars_read, tm_s.tm_year + 1900, tm_s.tm_mon + 1, tm_s.tm_mday, tm_s.tm_hour, tm_s.tm_min, tm_s.tm_sec, gpsu_pretty);
				GPMF_CopyState(ms, &gpsf_stream); // L
				if (GPMF_OK != GPMF_FindPrev(&gpsf_stream, STR2FOURCC("GPSF"), GPMF_CURRENT_LEVEL|GPMF_TOLERANT))
				{
					printf("Wtf whered it go GPSF\n");
					continue;
				}
				uint32_t gpsf;
				if (GPMF_OK != GPMF_ScaledData(&gpsf_stream, &gpsf, sizeof(uint32_t), 0, 1, GPMF_TYPE_UNSIGNED_LONG)) {
					printf("GPSF broke\n");
					continue;
				}

				char* rawdata = (char*)GPMF_RawData(ms);
				uint32_t key = GPMF_Key(ms);
				GPMF_SampleType type = GPMF_Type(ms);
				uint32_t samples = GPMF_Repeat(ms);
				uint32_t elements = GPMF_ElementsInStruct(ms);

				if (samples)
				{
					uint32_t buffersize = samples * elements * sizeof(double);
					GPMF_stream find_stream;
					double* ptr, * tmpbuffer = (double*)malloc(buffersize);

#define MAX_UNITS	64
#define MAX_UNITLEN	8
					char units[MAX_UNITS][MAX_UNITLEN] = { "" };
					uint32_t unit_samples = 1;

					char complextype[MAX_UNITS] = { "" };
					uint32_t type_samples = 1;

					if (tmpbuffer)
					{
						uint32_t i, j;

						//Search for any units to display
						GPMF_CopyState(ms, &find_stream);
						if (GPMF_OK == GPMF_FindPrev(&find_stream, GPMF_KEY_SI_UNITS, GPMF_CURRENT_LEVEL | GPMF_TOLERANT) ||
							GPMF_OK == GPMF_FindPrev(&find_stream, GPMF_KEY_UNITS, GPMF_CURRENT_LEVEL | GPMF_TOLERANT))
						{
							char* data = (char*)GPMF_RawData(&find_stream);
							uint32_t ssize = GPMF_StructSize(&find_stream);
							if (ssize > MAX_UNITLEN - 1) ssize = MAX_UNITLEN - 1;
							unit_samples = GPMF_Repeat(&find_stream);

							for (i = 0; i < unit_samples && i < MAX_UNITS; i++)
							{
								memcpy(units[i], data, ssize);
								units[i][ssize] = 0;
								data += ssize;
							}
						}

						//Search for TYPE if Complex
						GPMF_CopyState(ms, &find_stream);
						type_samples = 0;
						if (GPMF_OK == GPMF_FindPrev(&find_stream, GPMF_KEY_TYPE, GPMF_CURRENT_LEVEL | GPMF_TOLERANT))
						{
							char* data = (char*)GPMF_RawData(&find_stream);
							uint32_t ssize = GPMF_StructSize(&find_stream);
							if (ssize > MAX_UNITLEN - 1) ssize = MAX_UNITLEN - 1;
							type_samples = GPMF_Repeat(&find_stream);

							for (i = 0; i < type_samples && i < MAX_UNITS; i++)
							{
								complextype[i] = data[i];
							}
						}

						if (GPMF_OK == GPMF_ScaledData(ms, tmpbuffer, buffersize, 0, samples, GPMF_TYPE_DOUBLE))//Output scaled data as floats
						{

							ptr = tmpbuffer;
							int pos = 0;
							for (i = 0; i < samples; i++)
							{
								// Each CSV line
								printf("%c%c%c%c, %.10f, %.03f, %d",  PRINTF_4CC(key), base_time + (out-in)*((double)i/samples), gpsp/100.0, gpsf);

								for (j = 0; j < elements; j++)
								{
									// Each of the columns from the GPS5 Spec
									if (type == GPMF_TYPE_STRING_ASCII)
									{
										printf("%c", rawdata[pos]);
										pos++;
										ptr++;
									}
									else if (type_samples == 0) //no TYPE structure
									{
										// Prep for CSV. This is the only branch that should be used
										printf(", %.10f", *ptr++);
									}
									else if (complextype[j] != 'F')
									{
										printf("%.10f%s, ", *ptr++, units[j % unit_samples]);
										pos += GPMF_SizeofType((GPMF_SampleType)complextype[j]);
									}
									else if (type_samples && complextype[j] == GPMF_TYPE_FOURCC)
									{
										ptr++;
										printf("%c%c%c%c, ", rawdata[pos], rawdata[pos + 1], rawdata[pos + 2], rawdata[pos + 3]);
										pos += GPMF_SizeofType((GPMF_SampleType)complextype[j]);
									}
								}
								printf("\n");
							}
						}
						free(tmpbuffer);
					}
				}
			}
			GPMF_ResetState(ms);
		}

	cleanup:
		if (payloadres) FreePayloadResource(mp4handle, payloadres);
		if (ms) GPMF_Free(ms);
		CloseSource(mp4handle);
	}

	if (ret != 0)
	{
		if (GPMF_ERROR_UNKNOWN_TYPE == ret)
			printf("Unknown GPMF Type within\n");
		else
			printf("GPMF data has corruption\n");
	}

	return (int)ret;
}
