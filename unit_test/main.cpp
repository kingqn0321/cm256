/*
	Copyright (c) 2015 Christopher A. Taylor.  All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	* Redistributions of source code must retain the above copyright notice,
	  this list of conditions and the following disclaimer.
	* Redistributions in binary form must reproduce the above copyright notice,
	  this list of conditions and the following disclaimer in the documentation
	  and/or other materials provided with the distribution.
	* Neither the name of CM256 nor the names of its contributors may be
	  used to endorse or promote products derived from this software without
	  specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
	ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
	LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
	CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
*/

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable: 4530) // warning C4530: C++ exception handler used, but unwind semantics are not enabled. Specify /EHsc
#endif

#include <iostream>
using namespace std;

#ifdef _MSC_VER
    #pragma warning(pop)
#endif

#include "cm256.h"
#include "SiameseTools.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#include <chrono>
#include <thread>

void initializeBlocks(cm256_block originals[256], int blockCount, int blockBytes)
{
    for (int i = 0; i < blockCount; ++i)
    {
        for (int j = 0; j < blockBytes; ++j)
        {
            const uint8_t expected = (uint8_t)(i + j * 13);
            uint8_t* data = (uint8_t*)originals[i].Block;
            data[j] = expected;
        }
    }
}

bool validateSolution(cm256_block_t* blocks, int blockCount, int blockBytes)
{
    uint8_t seen[256] = { 0 };

    for (int i = 0; i < blockCount; ++i)
    {
        uint8_t index = blocks[i].Index;

        if (index >= blockCount)
        {
            return false;
        }

        if (seen[index])
        {
            return false;
        }

        seen[index] = 1;

        for (int j = 0; j < blockBytes; ++j)
        {
            const uint8_t expected = (uint8_t)(index + j * 13);
            uint8_t* blockData = (uint8_t*)blocks[i].Block;
            if (blockData[j] != expected)
            {
                return false;
            }
        }
    }

    return true;
}



bool ExampleFileUsage()
{
    if (cm256_init())
    {
        return false;
    }

    cm256_encoder_params params;

    // Number of bytes per file block
    params.BlockBytes = 1296;

    // Number of blocks
    params.OriginalCount = 100;

    // Number of additional recovery blocks generated by encoder
    params.RecoveryCount = 30;

    // Size of the original file
    static const int OriginalFileBytes = params.OriginalCount * params.BlockBytes;

    // Allocate and fill the original file data
    uint8_t* originalFileData = new uint8_t[OriginalFileBytes];
    for (int i = 0; i < OriginalFileBytes; ++i)
    {
        originalFileData[i] = (uint8_t)i;
    }

    // Pointers to data
    cm256_block blocks[256];
    for (int i = 0; i < params.OriginalCount; ++i)
    {
        blocks[i].Block = originalFileData + i * params.BlockBytes;
    }

    // Recovery data
    uint8_t* recoveryBlocks = new uint8_t[params.RecoveryCount * params.BlockBytes];

    // Generate recovery data
    if (cm256_encode(params, blocks, recoveryBlocks))
    {
        return false;
    }

    // Initialize the indices
    for (int i = 0; i < params.OriginalCount; ++i)
    {
        blocks[i].Index = cm256_get_original_block_index(params, i);
    }

    //// Simulate loss of data, substituting a recovery block in its place ////
    for (int i = 0; i < params.RecoveryCount && i < params.OriginalCount; ++i)
    {
        blocks[i].Block = recoveryBlocks + params.BlockBytes * i; // First recovery block
        blocks[i].Index = cm256_get_recovery_block_index(params, i); // First recovery block index
    }
    //// Simulate loss of data, substituting a recovery block in its place ////

    if (cm256_decode(params, blocks))
    {
        return false;
    }

    for (int i = 0; i < params.RecoveryCount && i < params.OriginalCount; ++i)
    {
        uint8_t* block = (uint8_t*)blocks[i].Block;
        int index = blocks[i].Index;

        for (int j = 0; j < params.BlockBytes; ++j)
        {
            const uint8_t expected = (uint8_t)(j + index * params.BlockBytes);
            if (block[j] != expected)
            {
                return false;
            }
        }
    }

    delete[] originalFileData;
    delete[] recoveryBlocks;

    return true;
}

bool CheckMemSwap()
{
    unsigned char buffa[16 + 8 + 4 + 3];
    memset(buffa, 1, sizeof(buffa));
    unsigned char buffb[16 + 8 + 4 + 3];
    memset(buffb, 2, sizeof(buffb));

    gf256_memswap(buffa, buffb, (int)sizeof(buffa));

    for (int i = 0; i < (int)sizeof(buffa); ++i)
    {
        if (buffa[i] != 2)
        {
            return false;
        }
        if (buffb[i] != 1)
        {
            return false;
        }
    }

    gf256_memswap(buffa, buffb, (int)sizeof(buffa));

    for (int i = 0; i < (int)sizeof(buffa); ++i)
    {
        if (buffa[i] != 1)
        {
            return false;
        }
        if (buffb[i] != 2)
        {
            return false;
        }
    }

    return true;
}

bool FinerPerfTimingTest()
{
#ifdef _WIN32
    ::SetPriorityClass(::GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
    ::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
#endif

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    if (cm256_init())
    {
        return false;
    }

    cm256_block blocks[256];

    uint64_t tsum_enc = 0;
    uint64_t tsum_dec = 0;

    cm256_encoder_params params;
    params.BlockBytes = 1400;
    params.OriginalCount = 48;
    params.RecoveryCount = 96;

    unsigned char* orig_data = new unsigned char[256 * params.BlockBytes];
    unsigned char* recoveryData = new unsigned char[256 * params.BlockBytes];

    const int trials = 1000;
    for (int trial = 0; trial < trials; ++trial)
    {
        for (int i = 0; i < params.BlockBytes * params.OriginalCount; ++i)
        {
            orig_data[i] = (uint8_t)i;
        }

        for (int i = 0; i < params.OriginalCount; ++i)
        {
            blocks[i].Block = orig_data + i * params.BlockBytes;
        }

        const uint64_t t0 = siamese::GetTimeUsec();
        if (cm256_encode(params, blocks, recoveryData))
        {
            return false;
        }

        const uint64_t t1 = siamese::GetTimeUsec();
        tsum_enc += t1 - t0;

        // Initialize the indices
        for (int i = 0; i < params.OriginalCount; ++i)
        {
            blocks[i].Index = cm256_get_original_block_index(params, i);
        }

        //// Simulate loss of data, substituting a recovery block in its place ////
        for (int i = 0; i < params.RecoveryCount && i < params.OriginalCount; ++i)
        {
            blocks[i].Block = recoveryData + params.BlockBytes * i; // First recovery block
            blocks[i].Index = cm256_get_recovery_block_index(params, i); // First recovery block index
        }
        //// Simulate loss of data, substituting a recovery block in its place ////

        const uint64_t t2 = siamese::GetTimeUsec();

        if (cm256_decode(params, blocks))
        {
            return false;
        }

        const uint64_t t3 = siamese::GetTimeUsec();
        tsum_dec += t3 - t2;

        for (int i = 0; i < params.RecoveryCount && i < params.OriginalCount; ++i)
        {
            uint8_t* block = (uint8_t*)blocks[i].Block;
            int index = blocks[i].Index;

            for (int j = 0; j < params.BlockBytes; ++j)
            {
                const uint8_t expected = (uint8_t)(j + index * params.BlockBytes);
                if (block[j] != expected)
                {
                    return false;
                }
            }
        }
    }

    const double opusec_enc = tsum_enc / static_cast<double>(trials);
    const double mbps_enc = (params.BlockBytes * params.OriginalCount / opusec_enc);
    const double opusec_dec = tsum_dec / static_cast<double>(trials);
    const double mbps_dec = (params.BlockBytes * params.OriginalCount / opusec_dec);

    cout << "Params: size = " << params.BlockBytes << " k = " << params.OriginalCount << " m = " << params.RecoveryCount << endl;
    cout << "Encoder: " << opusec_enc << " usec, " << mbps_enc << " MBps" << endl;
    cout << "Decoder: " << opusec_dec << " usec, " << mbps_dec << " MBps" << endl;

#ifdef _WIN32
    ::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_NORMAL);
    ::SetPriorityClass(::GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
#endif

    return true;
}


bool BulkPerfTesting()
{
    if (cm256_init())
    {
        return false;
    }

    static const int MaxBlockBytes = 10000; // multiple of 10

    unsigned char* orig_data = new unsigned char[256 * MaxBlockBytes];

    unsigned char* recoveryData = new unsigned char[256 * MaxBlockBytes];

    cm256_block blocks[256];

    for (int blockBytes = 8 * 162; blockBytes <= MaxBlockBytes; blockBytes *= 10)
    {
        for (int originalCount = 1; originalCount < 256; ++originalCount)
        {
            for (int recoveryCount = 1; recoveryCount <= 1 + originalCount / 2 && recoveryCount <= 256 - originalCount; ++recoveryCount)
            {
                cm256_encoder_params params;
                params.BlockBytes = blockBytes;
                params.OriginalCount = originalCount;
                params.RecoveryCount = recoveryCount;

                for (int i = 0; i < 256; ++i)
                {
                    blocks[i].Block = orig_data + i * MaxBlockBytes;
                }

                initializeBlocks(blocks, originalCount, blockBytes);

                {
                    const uint64_t t0 = siamese::GetTimeUsec();

                    if (cm256_encode(params, blocks, recoveryData))
                    {
                        cout << "Encoder error" << endl;
                        return false;
                    }

                    const uint64_t t1 = siamese::GetTimeUsec();
                    const int dt_usec = (int)static_cast<int64_t>( t1 - t0 );

                    const double opusec = dt_usec;
                    const double mbps = (params.BlockBytes * params.OriginalCount / opusec);

                    cout << "Encoder: " << blockBytes << " bytes k = " << originalCount << " m = " << recoveryCount << " : " << opusec << " usec, " << mbps << " MBps" << endl;
                }

                // Fill in indices
                for (int i = 0; i < originalCount; ++i)
                {
                    blocks[i].Index = cm256_get_original_block_index(params, i);
                }

                for (int ii = 0; ii < recoveryCount; ++ii)
                {
                    int erasure_index = recoveryCount - ii - 1;
                    blocks[ii].Block = recoveryData + erasure_index * blockBytes;
                    blocks[ii].Index = cm256_get_recovery_block_index(params, erasure_index);
                }

                {
                    const uint64_t t0 = siamese::GetTimeUsec();

                    if (cm256_decode(params, blocks))
                    {
                        cout << "Decoder error" << endl;
                        return false;
                    }

                    const uint64_t t1 = siamese::GetTimeUsec();
                    const int dt_usec = (int)static_cast<int64_t>( t1 - t0 );

                    const double opusec = dt_usec;
                    const double mbps = (params.BlockBytes * params.OriginalCount / opusec);

                    cout << "Decoder: " << blockBytes << " bytes k = " << originalCount << " m = " << recoveryCount << " : " << opusec << " usec, " << mbps << " MBps" << endl;
                }

                if (!validateSolution(blocks, originalCount, blockBytes))
                {
                    cout << "Solution invalid" << endl;
                    return false;
                }
            }
        }
    }

    return true;
}


int main()
{
#if 0
    if (!ExampleFileUsage())
    {
        exit(1);
    }
#endif
#if 0
    if (!CheckMemSwap())
    {
        exit(4);
    }
#endif
#if 1
    if (!FinerPerfTimingTest())
    {
        exit(2);
    }
#endif
#if 0
    if (!BulkPerfTesting())
    {
        exit(3);
    }
#endif

    return 0;
}
