#include <ultra64.h>

#include "synthesis.h"
#include "heap.h"
#include "data.h"
#include "load.h"
#include "seqplayer.h"
#include "external.h"

#ifndef TARGET_N64
#include "../pc/mixer.h"
#endif

#ifdef TARGET_N3DS
#include "src/pc/audio/audio_3ds.h"
static s16* sCurAiBufBasePtr = NULL;

#define DO_3DS(code) do { code } while(0)
#else
#define DO_3DS(code) do {} while(0)
#endif

#define DMEM_ADDR_TEMP 0x0
#define DMEM_ADDR_RESAMPLED 0x20
#define DMEM_ADDR_RESAMPLED2 0x160
#define DMEM_ADDR_UNCOMPRESSED_NOTE 0x180
#define DMEM_ADDR_NOTE_PAN_TEMP 0x200
#define DMEM_ADDR_STEREO_STRONG_TEMP_DRY 0x200
#define DMEM_ADDR_STEREO_STRONG_TEMP_WET 0x340
#define DMEM_ADDR_COMPRESSED_ADPCM_DATA 0x3f0
#define DMEM_ADDR_LEFT_CH 0x4c0
#define DMEM_ADDR_RIGHT_CH 0x600
#define DMEM_ADDR_WET_LEFT_CH 0x740
#define DMEM_ADDR_WET_RIGHT_CH 0x880

#define aSetLoadBufferPair(pkt, c, off)                                                                \
    aSetBuffer(pkt, 0, c + DMEM_ADDR_WET_LEFT_CH, 0, DEFAULT_LEN_1CH - c);                             \
    aLoadBuffer(pkt, VIRTUAL_TO_PHYSICAL2(gSynthesisReverb.ringBuffer.left + (off)));                  \
    aSetBuffer(pkt, 0, c + DMEM_ADDR_WET_RIGHT_CH, 0, DEFAULT_LEN_1CH - c);                            \
    aLoadBuffer(pkt, VIRTUAL_TO_PHYSICAL2(gSynthesisReverb.ringBuffer.right + (off)))

#define aSetSaveBufferPair(pkt, c, d, off)                                                             \
    aSetBuffer(pkt, 0, 0, c + DMEM_ADDR_WET_LEFT_CH, d);                                               \
    aSaveBuffer(pkt, VIRTUAL_TO_PHYSICAL2(gSynthesisReverb.ringBuffer.left +  (off)));                 \
    aSetBuffer(pkt, 0, 0, c + DMEM_ADDR_WET_RIGHT_CH, d);                                              \
    aSaveBuffer(pkt, VIRTUAL_TO_PHYSICAL2(gSynthesisReverb.ringBuffer.right + (off)));

// Rounds val up to the next multiple of (1 << amnt). For example, ALIGN(50, 5) results in 64 (nearest 32)
#define ALIGN(val, amnt) (((val) + (1 << amnt) - 1) & ~((1 << amnt) - 1))

struct VolumeChange {
    u16 sourceLeft;
    u16 sourceRight;
    u16 targetLeft;
    u16 targetRight;
};

u64 *synthesis_do_one_audio_update(s16 *aiBuf, s32 bufLen, u64 *cmd, u32 updateIndex);
#ifdef VERSION_EU
u64 *synthesis_process_note(struct Note *note, struct NoteSubEu *noteSubEu, struct NoteSynthesisState *synthesisState, s16 *aiBuf, s32 bufLen, u64 *cmd);
u64 *load_wave_samples(u64 *cmd, struct NoteSubEu *noteSubEu, struct NoteSynthesisState *synthesisState, s32 nSamplesToLoad);
u64 *final_resample(u64 *cmd, struct NoteSynthesisState *synthesisState, s32 count, u16 pitch, u16 dmemIn, u32 flags);
u64 *process_envelope(u64 *cmd, struct NoteSubEu *noteSubEu, struct NoteSynthesisState *synthesisState, s32 nSamples, u16 inBuf, s32 headsetPanSettings, u32 flags);
u64 *note_apply_headset_pan_effects(u64 *cmd, struct NoteSubEu *noteSubEu, struct NoteSynthesisState *note, s32 bufLen, s32 flags, s32 leftRight);
#else
u64 *synthesis_process_notes(s16 *aiBuf, s32 bufLen, u64 *cmd);
u64 *load_wave_samples(u64 *cmd, struct Note *note, s32 nSamplesToLoad);
u64 *final_resample(u64 *cmd, struct Note *note, s32 count, u16 pitch, u16 dmemIn, u32 flags);
u64 *process_envelope(u64 *cmd, struct Note *note, s32 nSamples, u16 inBuf, s32 headsetPanSettings,
                      u32 flags);
u64 *process_envelope_inner(u64 *cmd, struct Note *note, s32 nSamples, u16 inBuf,
                            s32 headsetPanSettings, struct VolumeChange *vol);
u64 *note_apply_headset_pan_effects(u64 *cmd, struct Note *note, s32 bufLen, s32 flags, s32 leftRight);
#endif

#ifdef VERSION_EU
struct SynthesisReverb gSynthesisReverbs[4];
u8 sAudioSynthesisPad[0x10];
s16 gVolume;
s8 gUseReverb;
s8 gNumSynthesisReverbs;
struct NoteSubEu *gNoteSubsEu;
f32 gLeftVolRampings[3][1024];
f32 gRightVolRampings[3][1024];
f32 *gCurrentLeftVolRamping; // Points to any of the three left buffers above
f32 *gCurrentRightVolRamping; // Points to any of the three right buffers above
#else
struct SynthesisReverb gSynthesisReverb;
#endif

#ifndef VERSION_EU
u8 sAudioSynthesisPad[0x20];
#endif

#if defined(VERSION_EU)
// Equivalent functionality as the US/JP version,
// just that the reverb structure is chosen from an array with index
void prepare_reverb_ring_buffer(s32 chunkLen, u32 updateIndex, s32 reverbIndex) {
    struct ReverbRingBufferItem *item;
    struct SynthesisReverb *reverb = &gSynthesisReverbs[reverbIndex];
    s32 srcPos;
    s32 dstPos;
    s32 nSamples;
    s32 excessiveSamples;
    s32 UNUSED pad[3];
    if (reverb->downsampleRate != 1) {
        if (reverb->framesLeftToIgnore == 0) {
            // Now that the RSP has finished, downsample the samples produced two frames ago by skipping
            // samples.
            item = &reverb->items[reverb->curFrame][updateIndex];

            // Touches both left and right since they are adjacent in memory
            osInvalDCache(item->toDownsampleLeft, DEFAULT_LEN_2CH);

            for (srcPos = 0, dstPos = 0; dstPos < item->lengthA / 2;
                 srcPos += reverb->downsampleRate, dstPos++) {
                reverb->ringBuffer.left[item->startPos + dstPos] =
                    item->toDownsampleLeft[srcPos];
                reverb->ringBuffer.right[item->startPos + dstPos] =
                    item->toDownsampleRight[srcPos];
            }
            for (dstPos = 0; dstPos < item->lengthB / 2; srcPos += reverb->downsampleRate, dstPos++) {
                reverb->ringBuffer.left[dstPos] = item->toDownsampleLeft[srcPos];
                reverb->ringBuffer.right[dstPos] = item->toDownsampleRight[srcPos];
            }
        }
    }

    item = &reverb->items[reverb->curFrame][updateIndex];
    nSamples = chunkLen / reverb->downsampleRate;
    excessiveSamples = (nSamples + reverb->nextRingBufferPos) - reverb->bufSizePerChannel;
    if (excessiveSamples < 0) {
        // There is space in the ring buffer before it wraps around
        item->lengthA = nSamples * 2;
        item->lengthB = 0;
        item->startPos = (s32) reverb->nextRingBufferPos;
        reverb->nextRingBufferPos += nSamples;
    } else {
        // Ring buffer wrapped around
        item->lengthA = (nSamples - excessiveSamples) * 2;
        item->lengthB = excessiveSamples * 2;
        item->startPos = reverb->nextRingBufferPos;
        reverb->nextRingBufferPos = excessiveSamples;
    }
    // These fields are never read later
    item->numSamplesAfterDownsampling = nSamples;
    item->chunkLen = chunkLen;
}
#else
void prepare_reverb_ring_buffer(s32 chunkLen, u32 updateIndex) {
    struct ReverbRingBufferItem *item;
    s32 srcPos;
    s32 dstPos;
    s32 nSamples;
    s32 numSamplesAfterDownsampling;
    s32 excessiveSamples;
    if (gReverbDownsampleRate != 1) {
        if (gSynthesisReverb.framesLeftToIgnore == 0) {
            // Now that the RSP has finished, downsample the samples produced two frames ago by skipping
            // samples.
            item = &gSynthesisReverb.items[gSynthesisReverb.curFrame][updateIndex];

            // Touches both left and right since they are adjacent in memory
            osInvalDCache(item->toDownsampleLeft, DEFAULT_LEN_2CH);

            for (srcPos = 0, dstPos = 0; dstPos < item->lengthA / 2;
                 srcPos += gReverbDownsampleRate, dstPos++) {
                gSynthesisReverb.ringBuffer.left[dstPos + item->startPos] =
                    item->toDownsampleLeft[srcPos];
                gSynthesisReverb.ringBuffer.right[dstPos + item->startPos] =
                    item->toDownsampleRight[srcPos];
            }
            for (dstPos = 0; dstPos < item->lengthB / 2; srcPos += gReverbDownsampleRate, dstPos++) {
                gSynthesisReverb.ringBuffer.left[dstPos] = item->toDownsampleLeft[srcPos];
                gSynthesisReverb.ringBuffer.right[dstPos] = item->toDownsampleRight[srcPos];
            }
        }
    }
    item = &gSynthesisReverb.items[gSynthesisReverb.curFrame][updateIndex];

    numSamplesAfterDownsampling = chunkLen / gReverbDownsampleRate;
    if (((numSamplesAfterDownsampling + gSynthesisReverb.nextRingBufferPos) - gSynthesisReverb.bufSizePerChannel) < 0) {
        // There is space in the ring buffer before it wraps around
        item->lengthA = numSamplesAfterDownsampling * 2;
        item->lengthB = 0;
        item->startPos = (s32) gSynthesisReverb.nextRingBufferPos;
        gSynthesisReverb.nextRingBufferPos += numSamplesAfterDownsampling;
    } else {
        // Ring buffer wrapped around
        excessiveSamples =
            (numSamplesAfterDownsampling + gSynthesisReverb.nextRingBufferPos) - gSynthesisReverb.bufSizePerChannel;
        nSamples = numSamplesAfterDownsampling - excessiveSamples;
        item->lengthA = nSamples * 2;
        item->lengthB = excessiveSamples * 2;
        item->startPos = gSynthesisReverb.nextRingBufferPos;
        gSynthesisReverb.nextRingBufferPos = excessiveSamples;
    }
    // These fields are never read later
    item->numSamplesAfterDownsampling = numSamplesAfterDownsampling;
    item->chunkLen = chunkLen;
}
#endif

#ifdef VERSION_EU
u64 *synthesis_load_reverb_ring_buffer(u64 *cmd, u16 addr, u16 srcOffset, s32 len, s32 reverbIndex) {
    // aSetBuffer, aLoadBuffer, aSetBuffer, aLoadBuffer
    aSetBuffer(cmd++, 0, addr, 0, len);
    aLoadBuffer(cmd++, VIRTUAL_TO_PHYSICAL2(&gSynthesisReverbs[reverbIndex].ringBuffer.left[srcOffset]));

    aSetBuffer(cmd++, 0, addr + DEFAULT_LEN_1CH, 0, len);
    aLoadBuffer(cmd++, VIRTUAL_TO_PHYSICAL2(&gSynthesisReverbs[reverbIndex].ringBuffer.right[srcOffset]));

    return cmd;
}


u64 *synthesis_save_reverb_ring_buffer(u64 *cmd, u16 addr, u16 destOffset, s32 len, s32 reverbIndex) {
    aSetBuffer(cmd++, 0, 0, addr, len);
    aSaveBuffer(cmd++, VIRTUAL_TO_PHYSICAL2(&gSynthesisReverbs[reverbIndex].ringBuffer.left[destOffset]));

    aSetBuffer(cmd++, 0, 0, addr + DEFAULT_LEN_1CH, len);
    aSaveBuffer(cmd++, VIRTUAL_TO_PHYSICAL2(&gSynthesisReverbs[reverbIndex].ringBuffer.right[destOffset]));

    return cmd;
}

void synthesis_load_note_subs_eu(s32 updateIndex) {
    struct NoteSubEu *src;
    struct NoteSubEu *dest;
    s32 i;

    for (i = 0; i < gMaxSimultaneousNotes; i++) {
        src = &gNotes[i].noteSubEu;
        dest = &gNoteSubsEu[gMaxSimultaneousNotes * updateIndex + i];
        if (src->enabled) {
            *dest = *src;
            src->needsInit = 0;
        } else {
            dest->enabled = 0;
        }
    }
}
#endif

#ifndef VERSION_EU
s32 get_volume_ramping(u16 sourceVol, u16 targetVol, s32 nSamples) {
    // This roughly computes 2^16 * (targetVol / sourceVol) ^ (8 / nSamples),
    // but with discretizations of targetVol, sourceVol and nSamples.
    f32 ret;
    switch (nSamples) {
        default:
            ret = gVolRampingLhs136[targetVol >> 8] * gVolRampingRhs136[sourceVol >> 8];
            break;
        case 128:
            ret = gVolRampingLhs128[targetVol >> 8] * gVolRampingRhs128[sourceVol >> 8];
            break;
        case 136:
            ret = gVolRampingLhs136[targetVol >> 8] * gVolRampingRhs136[sourceVol >> 8];
            break;
        case 144:
            ret = gVolRampingLhs144[targetVol >> 8] * gVolRampingRhs144[sourceVol >> 8];
            break;
    }
    return ret;
}
#endif

#ifdef VERSION_EU
//TODO: (Scrub C) pointless mask and whitespace
u64 *synthesis_execute(u64 *cmdBuf, s32 *writtenCmds, s16 *aiBuf, s32 bufLen) {
    s32 i, j;
    f32 *leftVolRamp;
    f32 *rightVolRamp;
    u32 *aiBufPtr;
    u64 *cmd = cmdBuf;
    s32 chunkLen;
    s32 nextVolRampTable;
    DO_3DS(sCurAiBufBasePtr = aiBuf;);

    for (i = gAudioBufferParameters.updatesPerFrame; i > 0; i--) {
        process_sequences(i - 1);
        synthesis_load_note_subs_eu(gAudioBufferParameters.updatesPerFrame - i);
    }
    aSegment(cmd++, 0, 0);
    aiBufPtr = (u32 *) aiBuf;
    for (i = gAudioBufferParameters.updatesPerFrame; i > 0; i--) {
        if (i == 1) {
            // self-assignment has no affect when added here, could possibly simplify a macro definition
            chunkLen = bufLen; nextVolRampTable = nextVolRampTable; leftVolRamp = gLeftVolRampings[nextVolRampTable]; rightVolRamp = gRightVolRampings[nextVolRampTable & 0xFFFFFFFF];
        } else {
            if (bufLen / i >= gAudioBufferParameters.samplesPerUpdateMax) {
                chunkLen = gAudioBufferParameters.samplesPerUpdateMax; nextVolRampTable = 2; leftVolRamp = gLeftVolRampings[2]; rightVolRamp = gRightVolRampings[2];
            } else if (bufLen / i <= gAudioBufferParameters.samplesPerUpdateMin) {
                chunkLen = gAudioBufferParameters.samplesPerUpdateMin; nextVolRampTable = 0; leftVolRamp = gLeftVolRampings[0]; rightVolRamp = gRightVolRampings[0];
            } else {
                chunkLen = gAudioBufferParameters.samplesPerUpdate; nextVolRampTable = 1; leftVolRamp = gLeftVolRampings[1]; rightVolRamp = gRightVolRampings[1];
            }
        }
        gCurrentLeftVolRamping = leftVolRamp;
        gCurrentRightVolRamping = rightVolRamp;
        for (j = 0; j < gNumSynthesisReverbs; j++) {
            if (gSynthesisReverbs[j].useReverb != 0) {
                prepare_reverb_ring_buffer(chunkLen, gAudioBufferParameters.updatesPerFrame - i, j);
            }
        }
        cmd = synthesis_do_one_audio_update((s16 *) aiBufPtr, chunkLen, cmd, gAudioBufferParameters.updatesPerFrame - i);
        bufLen -= chunkLen;
        aiBufPtr += chunkLen;
    }

    for (j = 0; j < gNumSynthesisReverbs; j++) {
        if (gSynthesisReverbs[j].framesLeftToIgnore != 0) {
            gSynthesisReverbs[j].framesLeftToIgnore--;
        }
        gSynthesisReverbs[j].curFrame ^= 1;
    }
    *writtenCmds = cmd - cmdBuf;
    return cmd;
}
#else

// NTSC versions
// bufLen will be divisible by 16
// bufLen is how many s16s are written
u64 *synthesis_execute(u64 *cmdBuf, s32 *writtenCmds, s16 *aiBuf, s32 bufLen) {
    s32 chunkLen;
    s32 i;
    u32 *aiBufPtr = (u32 *) aiBuf;
    u64 *cmd = cmdBuf + 1;
    s32 v0;
    DO_3DS(sCurAiBufBasePtr = aiBuf;);

    aSegment(cmdBuf, 0, 0);

    for (i = gAudioUpdatesPerFrame; i > 0; i--) {
        if (i == 1) {
            // 'bufLen' will automatically be divisible by 8, no need to round
            chunkLen = bufLen;
        } else {
            v0 = bufLen / i;
            // chunkLen = v0 rounded to nearest multiple of 8
            chunkLen = v0 - (v0 & 7);

            if ((v0 & 7) >= 4) {
                chunkLen += 8;
            }
        }

        process_sequences(i - 1);
        
        if (gSynthesisReverb.useReverb != 0) {
            prepare_reverb_ring_buffer(chunkLen, gAudioUpdatesPerFrame - i);
        }

        cmd = synthesis_do_one_audio_update((s16 *) aiBufPtr, chunkLen, cmd, gAudioUpdatesPerFrame - i);
        bufLen -= chunkLen;
        aiBufPtr += chunkLen;
    }
    if (gSynthesisReverb.framesLeftToIgnore != 0) {
        gSynthesisReverb.framesLeftToIgnore--;
    }
    gSynthesisReverb.curFrame ^= 1;
    *writtenCmds = cmd - cmdBuf;
    return cmd;
}
#endif


#ifdef VERSION_EU
u64 *synthesis_resample_and_mix_reverb(u64 *cmd, s32 bufLen, s16 reverbIndex, s16 updateIndex) {
    struct ReverbRingBufferItem *item;
    s16 startPad;
    s16 paddedLengthA;

    item = &gSynthesisReverbs[reverbIndex].items[gSynthesisReverbs[reverbIndex].curFrame][updateIndex];

    aClearBuffer(cmd++, DMEM_ADDR_WET_LEFT_CH, DEFAULT_LEN_2CH);
    if (gSynthesisReverbs[reverbIndex].downsampleRate == 1) {
        cmd = synthesis_load_reverb_ring_buffer(cmd, DMEM_ADDR_WET_LEFT_CH, item->startPos, item->lengthA, reverbIndex);
        if (item->lengthB != 0) {
            cmd = synthesis_load_reverb_ring_buffer(cmd, DMEM_ADDR_WET_LEFT_CH + item->lengthA, 0, item->lengthB, reverbIndex);
        }
        aSetBuffer(cmd++, 0, 0, 0, DEFAULT_LEN_2CH);
        aMix(cmd++, 0, 0x7fff, DMEM_ADDR_WET_LEFT_CH, DMEM_ADDR_LEFT_CH);
        aMix(cmd++, 0, 0x8000 + gSynthesisReverbs[reverbIndex].reverbGain, DMEM_ADDR_WET_LEFT_CH, DMEM_ADDR_WET_LEFT_CH);
    } else {
        startPad = (item->startPos % 8u) * 2;
        paddedLengthA = ALIGN(startPad + item->lengthA, 4);

        cmd = synthesis_load_reverb_ring_buffer(cmd, DMEM_ADDR_RESAMPLED, (item->startPos - startPad / 2), DEFAULT_LEN_1CH, reverbIndex);
        if (item->lengthB != 0) {
            cmd = synthesis_load_reverb_ring_buffer(cmd, DMEM_ADDR_RESAMPLED + paddedLengthA, 0, DEFAULT_LEN_1CH - paddedLengthA, reverbIndex);
        }

        aSetBuffer(cmd++, 0, DMEM_ADDR_RESAMPLED + startPad, DMEM_ADDR_WET_LEFT_CH, bufLen * 2);
        aResample(cmd++, gSynthesisReverbs[reverbIndex].resampleFlags, gSynthesisReverbs[reverbIndex].resampleRate, VIRTUAL_TO_PHYSICAL2(gSynthesisReverbs[reverbIndex].resampleStateLeft));

        aSetBuffer(cmd++, 0, DMEM_ADDR_RESAMPLED2 + startPad, DMEM_ADDR_WET_RIGHT_CH, bufLen * 2);
        aResample(cmd++, gSynthesisReverbs[reverbIndex].resampleFlags, gSynthesisReverbs[reverbIndex].resampleRate, VIRTUAL_TO_PHYSICAL2(gSynthesisReverbs[reverbIndex].resampleStateRight));

        aSetBuffer(cmd++, 0, 0, 0, DEFAULT_LEN_2CH);
        aMix(cmd++, 0, 0x7fff, DMEM_ADDR_WET_LEFT_CH, DMEM_ADDR_LEFT_CH);
        aMix(cmd++, 0, 0x8000 + gSynthesisReverbs[reverbIndex].reverbGain, DMEM_ADDR_WET_LEFT_CH, DMEM_ADDR_WET_LEFT_CH);
    }
    return cmd;
}

u64 *synthesis_save_reverb_samples(u64 *cmdBuf, s16 reverbIndex, s16 updateIndex) {
    struct ReverbRingBufferItem *item;
    struct SynthesisReverb *reverb;
    u64 *cmd = cmdBuf;

    reverb = &gSynthesisReverbs[reverbIndex];
    item = &reverb->items[reverb->curFrame][updateIndex];
    if (reverb->useReverb != 0) {
        if (1) {
        }
        if (reverb->downsampleRate == 1) {
            // Put the oldest samples in the ring buffer into the wet channels
            cmd = cmdBuf = synthesis_save_reverb_ring_buffer(cmd, DMEM_ADDR_WET_LEFT_CH, item->startPos, item->lengthA, reverbIndex);
            if (item->lengthB != 0) {
                // Ring buffer wrapped
                cmd = synthesis_save_reverb_ring_buffer(cmd, DMEM_ADDR_WET_LEFT_CH + item->lengthA, 0, item->lengthB, reverbIndex);
                cmdBuf = cmd;
            }
        } else {
            // Downsampling is done later by CPU when RSP is done, therefore we need to have double
            // buffering. Left and right buffers are adjacent in memory.
            aSetBuffer(cmdBuf++, 0, 0, DMEM_ADDR_WET_LEFT_CH, DEFAULT_LEN_2CH);
            aSaveBuffer(cmdBuf++, VIRTUAL_TO_PHYSICAL2(reverb->items[reverb->curFrame][updateIndex].toDownsampleLeft));
            reverb->resampleFlags = 0;
        }
    }
    return cmdBuf;
}
#endif

#ifdef VERSION_EU
u64 *synthesis_do_one_audio_update(s16 *aiBuf, s32 bufLen, u64 *cmd, u32 updateIndex) {
    struct NoteSubEu *noteSubEu;
    u8 noteIndices[56];
    s32 temp;
    s32 i;
    s16 j;
    s16 notePos = 0;

    if (gNumSynthesisReverbs == 0) {
        for (i = 0; i < gMaxSimultaneousNotes; i++) {
            temp = updateIndex;
            if (gNoteSubsEu[gMaxSimultaneousNotes * temp + i].enabled) {
                noteIndices[notePos++] = i;
            }
        }
    } else {
        for (j = 0; j < gNumSynthesisReverbs; j++) {
            for (i = 0; i < gMaxSimultaneousNotes; i++) {
                temp = updateIndex;
                noteSubEu = &gNoteSubsEu[gMaxSimultaneousNotes * temp + i];
                if (noteSubEu->enabled && j == noteSubEu->reverbIndex) {
                    noteIndices[notePos++] = i;
                }
            }
        }

        for (i = 0; i < gMaxSimultaneousNotes; i++) {
            temp = updateIndex;
            noteSubEu = &gNoteSubsEu[gMaxSimultaneousNotes * temp + i];
            if (noteSubEu->enabled && noteSubEu->reverbIndex >= gNumSynthesisReverbs) {
                noteIndices[notePos++] = i;
            }
        }
    }
    aClearBuffer(cmd++, DMEM_ADDR_LEFT_CH, DEFAULT_LEN_2CH);
    i = 0;
    for (j = 0; j < gNumSynthesisReverbs; j++) {
        gUseReverb = gSynthesisReverbs[j].useReverb;
        if (gUseReverb != 0) {
            cmd = synthesis_resample_and_mix_reverb(cmd, bufLen, j, updateIndex);
        }
        for (; i < notePos; i++) {
            temp = updateIndex;
            temp *= gMaxSimultaneousNotes;
            if (j == gNoteSubsEu[temp + noteIndices[i]].reverbIndex) {
                cmd = synthesis_process_note(&gNotes[noteIndices[i]],
                                             &gNoteSubsEu[temp + noteIndices[i]],
                                             &gNotes[noteIndices[i]].synthesisState,
                                             aiBuf, bufLen, cmd);
                continue;
            } else {
                break;
            }
        }
        if (gSynthesisReverbs[j].useReverb != 0) {
            cmd = synthesis_save_reverb_samples(cmd, j, updateIndex);
        }
    }
    for (; i < notePos; i++) {
        temp = updateIndex;
        temp *= gMaxSimultaneousNotes;
        if (IS_BANK_LOAD_COMPLETE(gNoteSubsEu[temp + noteIndices[i]].bankId) == TRUE) {
            cmd = synthesis_process_note(&gNotes[noteIndices[i]],
                                         &gNoteSubsEu[temp + noteIndices[i]],
                                         &gNotes[noteIndices[i]].synthesisState,
                                         aiBuf, bufLen, cmd);
        } else {
            gAudioErrorFlags = (gNoteSubsEu[temp + noteIndices[i]].bankId + (i << 8)) + 0x10000000;
        }
    }

    temp = bufLen * 2;

// If possible, interleave directly into the output buf and skip the discrete copy.
#ifdef ENHANCED_RSPA_EMULATION
    aSetBuffer(cmd++, 0, 0, DMEM_ADDR_TEMP, temp);

    // Avoid redundant copy on 3DS when possible
    DO_3DS(
        if (audio_3ds_next_buffer_is_ready())
            aiBuf = direct_buf + (aiBuf - sCurAiBufBasePtr);
        else
            samples_to_copy += bufLen;
    );

    aInterleaveAndCopy(cmd++, DMEM_ADDR_LEFT_CH, DMEM_ADDR_RIGHT_CH, aiBuf);

// Else if enhanced RSPA Emulation is disabled, move to the copy buf.
#else
    aSetBuffer(cmd++, 0, 0, DMEM_ADDR_TEMP, temp);
    aInterleave(cmd++, DMEM_ADDR_LEFT_CH, DMEM_ADDR_RIGHT_CH);
    aSetBuffer(cmd++, 0, 0, DMEM_ADDR_TEMP, temp * 2);

    // Avoid redundant copy on 3DS when possible
    DO_3DS(
        if (audio_3ds_next_buffer_is_ready())
            aiBuf = direct_buf + (aiBuf - sCurAiBufBasePtr);
        else
            samples_to_copy += bufLen;
    );

    aSaveBuffer(cmd++, VIRTUAL_TO_PHYSICAL2(aiBuf));
#endif

    return cmd;
}

#else
u64 *synthesis_do_one_audio_update(s16 *aiBuf, s32 bufLen, u64 *cmd, u32 updateIndex) {
    UNUSED s32 pad1[1];
    s16 ra;
    s16 t4;
    UNUSED s32 pad[2];
    struct ReverbRingBufferItem *v1;
    UNUSED s32 pad2[1];
    s16 temp;

    v1 = &gSynthesisReverb.items[gSynthesisReverb.curFrame][updateIndex];

    if (gSynthesisReverb.useReverb == 0) {
        aClearBuffer(cmd++, DMEM_ADDR_LEFT_CH, DEFAULT_LEN_2CH);
        cmd = synthesis_process_notes(aiBuf, bufLen, cmd);
    } else {
        if (gReverbDownsampleRate == 1) {
            // Put the oldest samples in the ring buffer into the wet channels
            aSetLoadBufferPair(cmd++, 0, v1->startPos);
            if (v1->lengthB != 0) {
                // Ring buffer wrapped
                aSetLoadBufferPair(cmd++, v1->lengthA, 0);
                temp = 0;
            }

            // Use the reverb sound as initial sound for this audio update
            aDMEMMove(cmd++, DMEM_ADDR_WET_LEFT_CH, DMEM_ADDR_LEFT_CH, DEFAULT_LEN_2CH);

            // (Hopefully) lower the volume of the wet channels. New reverb will later be mixed into
            // these channels.
            aSetBuffer(cmd++, 0, 0, 0, DEFAULT_LEN_2CH);
            // 0x8000 here is -100%
            aMix(cmd++, 0, /*gain*/ 0x8000 + gSynthesisReverb.reverbGain, /*in*/ DMEM_ADDR_WET_LEFT_CH,
                 /*out*/ DMEM_ADDR_WET_LEFT_CH);
        } else {
            // Same as above but upsample the previously downsampled samples used for reverb first
            temp = 0; //! jesus christ
            t4 = (v1->startPos & 7) * 2;
            ra = ALIGN(v1->lengthA + t4, 4);
            aSetLoadBufferPair(cmd++, 0, v1->startPos - t4 / 2);
            if (v1->lengthB != 0) {
                // Ring buffer wrapped
                aSetLoadBufferPair(cmd++, ra, 0);
                //! We need an empty statement (even an empty ';') here to make the function match (because IDO).
                //! However, copt removes extraneous statements and dead code. So we need to trick copt
                //! into thinking 'temp' could be undefined, and luckily the compiler optimizes out the
                //! useless assignment.
                ra = ra + temp;
            }
            aSetBuffer(cmd++, 0, t4 + DMEM_ADDR_WET_LEFT_CH, DMEM_ADDR_LEFT_CH, bufLen << 1);
            aResample(cmd++, gSynthesisReverb.resampleFlags, (u16) gSynthesisReverb.resampleRate, VIRTUAL_TO_PHYSICAL2(gSynthesisReverb.resampleStateLeft));
            aSetBuffer(cmd++, 0, t4 + DMEM_ADDR_WET_RIGHT_CH, DMEM_ADDR_RIGHT_CH, bufLen << 1);
            aResample(cmd++, gSynthesisReverb.resampleFlags, (u16) gSynthesisReverb.resampleRate, VIRTUAL_TO_PHYSICAL2(gSynthesisReverb.resampleStateRight));
            aSetBuffer(cmd++, 0, 0, 0, DEFAULT_LEN_2CH);
            aMix(cmd++, 0, /*gain*/ 0x8000 + gSynthesisReverb.reverbGain, /*in*/ DMEM_ADDR_LEFT_CH, /*out*/ DMEM_ADDR_LEFT_CH);
            aDMEMMove(cmd++, DMEM_ADDR_LEFT_CH, DMEM_ADDR_WET_LEFT_CH, DEFAULT_LEN_2CH);
        }
        cmd = synthesis_process_notes(aiBuf, bufLen, cmd);
        if (gReverbDownsampleRate == 1) {
            aSetSaveBufferPair(cmd++, 0, v1->lengthA, v1->startPos);
            if (v1->lengthB != 0) {
                // Ring buffer wrapped
                aSetSaveBufferPair(cmd++, v1->lengthA, v1->lengthB, 0);
            }
        } else {
            // Downsampling is done later by CPU when RSP is done, therefore we need to have double
            // buffering. Left and right buffers are adjacent in memory.
            aSetBuffer(cmd++, 0, 0, DMEM_ADDR_WET_LEFT_CH, DEFAULT_LEN_2CH);
            aSaveBuffer(cmd++, VIRTUAL_TO_PHYSICAL2(gSynthesisReverb.items[gSynthesisReverb.curFrame][updateIndex].toDownsampleLeft));
            gSynthesisReverb.resampleFlags = 0;
        }
    }
    return cmd;
}
#endif

// All N64 versions
#ifdef TARGET_N64

#ifdef VERSION_EU
// Processes just one note, not all
u64 *synthesis_process_note(struct Note *note, struct NoteSubEu *noteSubEu, struct NoteSynthesisState *synthesisState, UNUSED s16 *aiBuf, s32 bufLen, u64 *cmd) {
    UNUSED s32 pad0[3];
#else
u64 *synthesis_process_notes(s16 *aiBuf, s32 bufLen, u64 *cmd) {
    s32 noteIndex;                           // sp174
    struct Note *note;                       // s7
    UNUSED u8 pad0[0x08];
#endif
    struct AudioBankSample *audioBookSample; // sp164, sp138
    struct AdpcmLoop *loopInfo;              // sp160, sp134
    s16 *curLoadedBook = NULL;               // sp154, samplePosAlignmentOffset
#ifdef VERSION_EU
    UNUSED u8 padEU[0x04];
#endif
    UNUSED u8 pad8[0x04];
#ifndef VERSION_EU
    u16 resamplingRateFixedPoint;            // sp5c, sp11A
#endif
    s32 noteFinished;                        // 150 t2, sp124
    s32 restart;                             // 14c t3, sp120
    s32 flags;                               // sp148, sp11C
#ifdef VERSION_EU
    u16 resamplingRateFixedPoint;            // sp5c, sp11A
#endif
    UNUSED u8 pad7[0x0c];                    // sp100
    UNUSED s32 tempBufLen;
#ifdef VERSION_EU
    s32 samplePosAlignmentOffset;  //sp128, sp104
    UNUSED u32 pad9;
#else
    UNUSED u32 pad9;
    s32 samplePosAlignmentOffset;  //sp128, sp104
#endif
    s32 nAdpcmSamplesProcessed; // signed required for US
    s32 nAdpcmPacketsThisIteration;
#ifdef VERSION_EU
    u8 *sampleAddr;                          // sp120, spF4
    s32 samplesSkippedThisIteration;
#else
    s32 samplesSkippedThisIteration;
    u8 *sampleAddr;                          // sp120, spF4
#endif

    // sp6c is a temporary!

#ifdef VERSION_EU
    s32 samplesLenAdjusted; // 108,      spEC
    // Might have been used to store (samplesLenFixedPoint >> 0x10), but doing so causes strange
    // behavior with the break near the end of the loop, causing US and JP to need a goto instead
    UNUSED s32 samplesLenInt;
    s32 endPos;             // sp110,    spE4
    s32 nSamplesToProcess;  // sp10c/a0, spE0
    s32 samplePosIntLowerNibble;
#else
    // Might have been used to store (samplesLenFixedPoint >> 0x10), but doing so causes strange
    // behavior with the break near the end of the loop, causing US and JP to need a goto instead
    UNUSED s32 samplesLenInt;
    s32 samplesLenAdjusted; // 108
    s32 samplePosIntLowerNibble;
    s32 endPos;             // sp110,    spE4
    s32 nSamplesToProcess;  // sp10c/a0, spE0
#endif

    s32 nUncompressedSamplesThisIteration;
    s32 s3;
    s32 decodeTailPtr; //s4

    u32 samplesLenFixedPoint;    // v1_1
    s32 nSamplesInThisIteration; // v1_2
    u32 sampleDataOffset;
#ifndef VERSION_EU
    s32 t9;
#endif
    u8 *sampleDataAddr;
    s32 nParts;                 // spE8, spBC
    s32 curPart;                // spE4, spB8

#ifndef VERSION_EU
    f32 resamplingRate; // f12
#endif
    s32 temp;

#ifdef VERSION_EU
    s32 decodeTailPtrAligned;
#endif
    s32 resampledTempLen;                    // spD8, spAC
    u16 noteSamplesDmemAddrBeforeResampling; // spD6, spAA


#ifndef VERSION_EU
    for (noteIndex = 0; noteIndex < gMaxSimultaneousNotes; noteIndex++) {
        note = &gNotes[noteIndex];
#ifdef VERSION_US
        //! This function requires note->enabled to be volatile, but it breaks other functions like note_enable.
        //! Casting to a struct with just the volatile bitfield works, but there may be a better way to match.
        if (((struct vNote *)note)->enabled && IS_BANK_LOAD_COMPLETE(note->bankId) == FALSE) {
#else
        if (IS_BANK_LOAD_COMPLETE(note->bankId) == FALSE) {
#endif
            gAudioErrorFlags = (note->bankId << 8) + noteIndex + 0x1000000;
        } else if (((struct vNote *)note)->enabled) {
#else
        if (note->noteSubEu.enabled == FALSE) {
            return cmd;
        } else {
#endif
            flags = 0;
#ifdef VERSION_EU
            tempBufLen = bufLen;
#endif

#ifdef VERSION_EU
            if (noteSubEu->needsInit == TRUE) {
#else
            if (note->needsInit == TRUE) {
#endif
                flags = A_INIT;
#ifndef VERSION_EU
                note->samplePosInt = 0;
                note->samplePosFrac = 0;
#else
                synthesisState->restart = FALSE;
                synthesisState->samplePosInt = 0;
                synthesisState->samplePosFrac = 0;
                synthesisState->curVolLeft = 1;
                synthesisState->curVolRight = 1;
                synthesisState->prevHeadsetPanRight = 0;
                synthesisState->prevHeadsetPanLeft = 0;
#endif
            }

#ifndef VERSION_EU
            if (note->frequency < US_FLOAT(2.0)) {
                nParts = 1;
                if (note->frequency > US_FLOAT(1.99996)) {
                    note->frequency = US_FLOAT(1.99996);
                }
                resamplingRate = note->frequency;
            } else {
                // If frequency is > 2.0, the processing must be split into two parts
                nParts = 2;
                if (note->frequency >= US_FLOAT(3.99993)) {
                    note->frequency = US_FLOAT(3.99993);
                }
                resamplingRate = note->frequency * US_FLOAT(.5);
            }

            resamplingRateFixedPoint = (u16)(s32)(resamplingRate * 32768.0f);
            samplesLenFixedPoint = note->samplePosFrac + (resamplingRateFixedPoint * bufLen) * 2;
            note->samplePosFrac = samplesLenFixedPoint & 0xFFFF; // 16-bit store, can't reuse
#else
            resamplingRateFixedPoint = noteSubEu->resamplingRateFixedPoint;
            nParts = noteSubEu->hasTwoAdpcmParts + 1;
            samplesLenFixedPoint = (resamplingRateFixedPoint * tempBufLen * 2) + synthesisState->samplePosFrac;
            synthesisState->samplePosFrac = samplesLenFixedPoint & 0xFFFF;
#endif

#ifdef VERSION_EU
            if (noteSubEu->isSyntheticWave) {
                cmd = load_wave_samples(cmd, noteSubEu, synthesisState, samplesLenFixedPoint >> 0x10);
                noteSamplesDmemAddrBeforeResampling = (synthesisState->samplePosInt * 2) + DMEM_ADDR_UNCOMPRESSED_NOTE;
                synthesisState->samplePosInt += samplesLenFixedPoint >> 0x10;
            }
#else
            if (note->sound == NULL) {
                // A wave synthesis note (not ADPCM)

                cmd = load_wave_samples(cmd, note, samplesLenFixedPoint >> 0x10);
                noteSamplesDmemAddrBeforeResampling = DMEM_ADDR_UNCOMPRESSED_NOTE + note->samplePosInt * 2;
                note->samplePosInt += (samplesLenFixedPoint >> 0x10);
                flags = 0;
            }
#endif
            else {
                // ADPCM note

#ifdef VERSION_EU
                audioBookSample = noteSubEu->sound.audioBankSound->sample;
#else
                audioBookSample = note->sound->sample;
#endif

                loopInfo = audioBookSample->loop;
                endPos = loopInfo->end;
                sampleAddr = audioBookSample->sampleAddr;
                resampledTempLen = 0;
                for (curPart = 0; curPart < nParts; curPart++) {
                    nAdpcmSamplesProcessed = 0; // s8
                    decodeTailPtr = 0;                     // s4

                    if (nParts == 1) {
                        samplesLenAdjusted = samplesLenFixedPoint >> 0x10;
                    } else if ((samplesLenFixedPoint >> 0x10) & 1) {
                        samplesLenAdjusted = ((samplesLenFixedPoint >> 0x10) & ~1) + (curPart * 2);
                    }
                    else {
                        samplesLenAdjusted = (samplesLenFixedPoint >> 0x10);
                    }

                    if (curLoadedBook != audioBookSample->book->book) {
                        u32 nEntries; // v1
                        curLoadedBook = audioBookSample->book->book;
#ifdef VERSION_EU
                        nEntries = 16 * audioBookSample->book->order * audioBookSample->book->npredictors;
                        aLoadADPCM(cmd++, nEntries, VIRTUAL_TO_PHYSICAL2(curLoadedBook + noteSubEu->bookOffset));
#else
                        nEntries = audioBookSample->book->order * audioBookSample->book->npredictors;
                        aLoadADPCM(cmd++, nEntries * 16, VIRTUAL_TO_PHYSICAL2(curLoadedBook));
#endif
                    }

#ifdef VERSION_EU
                    if (noteSubEu->bookOffset) {
                        curLoadedBook = (s16 *) &euUnknownData_80301950; // what's this? never read
                    }
#endif

                    while (nAdpcmSamplesProcessed != samplesLenAdjusted) {
                        s32 samplesRemaining; // v1
                        s32 nUncompressedSamplesThisIteration;

                        noteFinished = FALSE;
                        restart = FALSE;
                        nSamplesToProcess = samplesLenAdjusted - nAdpcmSamplesProcessed;
#ifdef VERSION_EU
                        samplePosIntLowerNibble = synthesisState->samplePosInt & 0xf;
                        samplesRemaining = endPos - synthesisState->samplePosInt;
#else
                        samplePosIntLowerNibble = note->samplePosInt & 0xf;
                        samplesRemaining = endPos - note->samplePosInt;
#endif

#ifdef VERSION_EU
                        if (samplePosIntLowerNibble == 0 && synthesisState->restart == FALSE) {
                            samplePosIntLowerNibble = 16;
                        }
#else
                        if (samplePosIntLowerNibble == 0 && note->restart == FALSE) {
                            samplePosIntLowerNibble = 16;
                        }
#endif
                        samplesSkippedThisIteration = 16 - samplePosIntLowerNibble; // a1

                        if (nSamplesToProcess < samplesRemaining) {
                            nAdpcmPacketsThisIteration = (nSamplesToProcess - samplesSkippedThisIteration + 0xf) / 16;
                            nUncompressedSamplesThisIteration = nAdpcmPacketsThisIteration * 16;
                            s3 = samplesSkippedThisIteration + nUncompressedSamplesThisIteration - nSamplesToProcess;
                        } else {
#ifndef VERSION_EU
                            nUncompressedSamplesThisIteration = samplesRemaining + samplePosIntLowerNibble - 0x10;
#else
                            nUncompressedSamplesThisIteration = samplesRemaining - samplesSkippedThisIteration;
#endif
                            s3 = 0;
                            if (nUncompressedSamplesThisIteration <= 0) {
                                nUncompressedSamplesThisIteration = 0;
                                samplesSkippedThisIteration = samplesRemaining;
                            }
                            nAdpcmPacketsThisIteration = (nUncompressedSamplesThisIteration + 0xf) / 16;
                            if (loopInfo->count != 0) {
                                // Loop around and restart
                                restart = 1;
                            } else {
                                noteFinished = 1;
                            }
                        }

                        if (nAdpcmPacketsThisIteration != 0) {
#ifdef VERSION_EU
                            temp = (synthesisState->samplePosInt - samplePosIntLowerNibble + 0x10) / 16;
                            if (audioBookSample->loaded == 0x81) {
                                sampleDataAddr = sampleAddr + temp * 9;
                            } else {
                                sampleDataAddr = dma_sample_data(
                                    (uintptr_t) (sampleAddr + temp * 9),
                                    nAdpcmPacketsThisIteration * 9, flags, &synthesisState->sampleDmaIndex);
                            }
#else
                            temp = (note->samplePosInt - samplePosIntLowerNibble + 0x10) / 16;
                            sampleDataAddr = dma_sample_data(
                                (uintptr_t) (sampleAddr + temp * 9),
                                nAdpcmPacketsThisIteration * 9, flags, &note->sampleDmaIndex);
#endif
                            sampleDataOffset = (u32)((uintptr_t) sampleDataAddr & 0xf);
                            aSetBuffer(cmd++, 0, DMEM_ADDR_COMPRESSED_ADPCM_DATA, 0, nAdpcmPacketsThisIteration * 9 + sampleDataOffset);
                            aLoadBuffer(cmd++, VIRTUAL_TO_PHYSICAL2(sampleDataAddr - sampleDataOffset));
                        } else {
                            nUncompressedSamplesThisIteration = 0;
                            sampleDataOffset = 0;
                        }

#ifdef VERSION_EU
                        if (synthesisState->restart != FALSE) {
                            aSetLoop(cmd++, VIRTUAL_TO_PHYSICAL2(audioBookSample->loop->state));
                            flags = A_LOOP; // = 2
                            synthesisState->restart = FALSE;
                        }
#else
                        if (note->restart != FALSE) {
                            aSetLoop(cmd++, VIRTUAL_TO_PHYSICAL2(audioBookSample->loop->state));
                            flags = A_LOOP; // = 2
                            note->restart = FALSE;
                        }
#endif

                        nSamplesInThisIteration = nUncompressedSamplesThisIteration + samplesSkippedThisIteration - s3;
#ifdef VERSION_EU
                        if (nAdpcmSamplesProcessed == 0) {
                            aSetBuffer(cmd++, 0, DMEM_ADDR_COMPRESSED_ADPCM_DATA + sampleDataOffset,
                                       DMEM_ADDR_UNCOMPRESSED_NOTE, nUncompressedSamplesThisIteration * 2);
                            aADPCMdec(cmd++, flags,
                                      VIRTUAL_TO_PHYSICAL2(synthesisState->synthesisBuffers->adpcmdecState));
                            samplePosAlignmentOffset = samplePosIntLowerNibble * 2;
                        } else {
                            decodeTailPtrAligned = ALIGN(decodeTailPtr, 5);
                            aSetBuffer(cmd++, 0, DMEM_ADDR_COMPRESSED_ADPCM_DATA + sampleDataOffset,
                                       DMEM_ADDR_UNCOMPRESSED_NOTE + decodeTailPtrAligned, nUncompressedSamplesThisIteration * 2);
                            aADPCMdec(cmd++, flags,
                                      VIRTUAL_TO_PHYSICAL2(synthesisState->synthesisBuffers->adpcmdecState));
                            aDMEMMove(cmd++, DMEM_ADDR_UNCOMPRESSED_NOTE + decodeTailPtrAligned + (samplePosIntLowerNibble * 2),
                                      DMEM_ADDR_UNCOMPRESSED_NOTE + decodeTailPtr, (nSamplesInThisIteration) * 2);
                        }
#else
                        if (nAdpcmSamplesProcessed == 0) {
                            aSetBuffer(cmd++, 0, DMEM_ADDR_COMPRESSED_ADPCM_DATA + sampleDataOffset, DMEM_ADDR_UNCOMPRESSED_NOTE, nUncompressedSamplesThisIteration * 2);
                            aADPCMdec(cmd++, flags, VIRTUAL_TO_PHYSICAL2(note->synthesisBuffers->adpcmdecState));
                            samplePosAlignmentOffset = samplePosIntLowerNibble * 2;
                        } else {
                            aSetBuffer(cmd++, 0, DMEM_ADDR_COMPRESSED_ADPCM_DATA + sampleDataOffset, DMEM_ADDR_UNCOMPRESSED_NOTE + ALIGN(decodeTailPtr, 5), nUncompressedSamplesThisIteration * 2);
                            aADPCMdec(cmd++, flags, VIRTUAL_TO_PHYSICAL2(note->synthesisBuffers->adpcmdecState));
                            aDMEMMove(cmd++, DMEM_ADDR_UNCOMPRESSED_NOTE + ALIGN(decodeTailPtr, 5) + (samplePosIntLowerNibble * 2), DMEM_ADDR_UNCOMPRESSED_NOTE + decodeTailPtr, (nSamplesInThisIteration) * 2);
                        }
#endif

                        nAdpcmSamplesProcessed += nSamplesInThisIteration;

                        switch (flags) {
                            case A_INIT: // = 1
                                samplePosAlignmentOffset = 0;
                                decodeTailPtr = nUncompressedSamplesThisIteration * 2 + decodeTailPtr;
                                break;

                            case A_LOOP: // = 2
                                decodeTailPtr = nSamplesInThisIteration * 2 + decodeTailPtr;
                                break;

                            default:
                                if (decodeTailPtr != 0) {
                                    decodeTailPtr = nSamplesInThisIteration * 2 + decodeTailPtr;
                                } else {
                                    decodeTailPtr = (samplePosIntLowerNibble + nSamplesInThisIteration) * 2;
                                }
                                break;
                        }
                        flags = 0;

                        if (noteFinished) {
                            aClearBuffer(cmd++, DMEM_ADDR_UNCOMPRESSED_NOTE + decodeTailPtr,
                                         (samplesLenAdjusted - nAdpcmSamplesProcessed) * 2);
#ifdef VERSION_EU
                            noteSubEu->finished = 1;
                            note->noteSubEu.finished = 1;
                            note->noteSubEu.enabled = 0;
#else
                            note->samplePosInt = 0;
                            note->finished = 1;
                            ((struct vNote *)note)->enabled = 0;
#endif
                            break;
                        }
#ifdef VERSION_EU
                        if (restart) {
                            synthesisState->restart = TRUE;
                            synthesisState->samplePosInt = loopInfo->start;
                        } else {
                            synthesisState->samplePosInt += nSamplesToProcess;
                        }
#else
                        if (restart) {
                            note->restart = TRUE;
                            note->samplePosInt = loopInfo->start;
                        } else {
                            note->samplePosInt += nSamplesToProcess;
                        }
#endif
                    }

                    switch (nParts) {
                        case 1:
                            noteSamplesDmemAddrBeforeResampling = DMEM_ADDR_UNCOMPRESSED_NOTE + samplePosAlignmentOffset;
                            break;

                        case 2:
                            switch (curPart) {
                                case 0:
                                    aSetBuffer(cmd++, 0, DMEM_ADDR_UNCOMPRESSED_NOTE + samplePosAlignmentOffset, DMEM_ADDR_RESAMPLED, samplesLenAdjusted + 4);
#ifdef VERSION_EU
                                    aResample(cmd++, A_INIT, 0xff60, VIRTUAL_TO_PHYSICAL2(synthesisState->synthesisBuffers->dummyResampleState));
#else
                                    aResample(cmd++, A_INIT, 0xff60, VIRTUAL_TO_PHYSICAL2(note->synthesisBuffers->dummyResampleState));
#endif
                                    resampledTempLen = samplesLenAdjusted + 4;
                                    noteSamplesDmemAddrBeforeResampling = DMEM_ADDR_RESAMPLED + 4;
#ifdef VERSION_EU
                                    if (noteSubEu->finished != FALSE) {
#else
                                    if (note->finished != FALSE) {
#endif
                                        aClearBuffer(cmd++, DMEM_ADDR_RESAMPLED + resampledTempLen, samplesLenAdjusted + 0x10);
                                    }
                                    break;

                                case 1:
                                    aSetBuffer(cmd++, 0, DMEM_ADDR_UNCOMPRESSED_NOTE + samplePosAlignmentOffset,
                                               DMEM_ADDR_RESAMPLED2,
                                               samplesLenAdjusted + 8);
#ifdef VERSION_EU
                                    aResample(cmd++, A_INIT, 0xff60,
                                              VIRTUAL_TO_PHYSICAL2(
                                                  synthesisState->synthesisBuffers->dummyResampleState));
#else
                                    aResample(cmd++, A_INIT, 0xff60,
                                              VIRTUAL_TO_PHYSICAL2(
                                                  note->synthesisBuffers->dummyResampleState));
#endif
                                    aDMEMMove(cmd++, DMEM_ADDR_RESAMPLED2 + 4,
                                              DMEM_ADDR_RESAMPLED + resampledTempLen,
                                              samplesLenAdjusted + 4);
                                    break;
                            }
                    }

#ifdef VERSION_EU
                    if (noteSubEu->finished != FALSE) {
#else
                    if (note->finished != FALSE) {
#endif
                        break;
                    }
                }
            }

            flags = 0;

#ifdef VERSION_EU
            if (noteSubEu->needsInit == TRUE) {
                flags = A_INIT;
                noteSubEu->needsInit = FALSE;
            }

            cmd = final_resample(cmd, synthesisState, bufLen * 2, resamplingRateFixedPoint,
                                 noteSamplesDmemAddrBeforeResampling, flags);
#else
            if (note->needsInit == TRUE) {
                flags = A_INIT;
                note->needsInit = FALSE;
            }

            cmd = final_resample(cmd, note, bufLen * 2, resamplingRateFixedPoint,
                                 noteSamplesDmemAddrBeforeResampling, flags);
#endif

#ifndef VERSION_EU
            if (note->headsetPanRight != 0 || note->prevHeadsetPanRight != 0) {
                nUncompressedSamplesThisIteration = 1;
            } else if (note->headsetPanLeft != 0 || note->prevHeadsetPanLeft != 0) {
                nUncompressedSamplesThisIteration = 2;
#else
            if (noteSubEu->headsetPanRight != 0 || synthesisState->prevHeadsetPanRight != 0) {
                nUncompressedSamplesThisIteration = 1;
            } else if (noteSubEu->headsetPanLeft != 0 || synthesisState->prevHeadsetPanLeft != 0) {
                nUncompressedSamplesThisIteration = 2;
#endif
            } else {
                nUncompressedSamplesThisIteration = 0;
            }

#ifdef VERSION_EU
            cmd = process_envelope(cmd, noteSubEu, synthesisState, bufLen, DMEM_ADDR_TEMP, nUncompressedSamplesThisIteration, flags);
#else
            cmd = process_envelope(cmd, note, bufLen, DMEM_ADDR_TEMP, nUncompressedSamplesThisIteration, flags);
#endif

#ifdef VERSION_EU
            if (noteSubEu->usesHeadsetPanEffects) {
                cmd = note_apply_headset_pan_effects(cmd, noteSubEu, synthesisState, bufLen * 2, flags, nUncompressedSamplesThisIteration);
            }
#else
            if (note->usesHeadsetPanEffects) {
                cmd = note_apply_headset_pan_effects(cmd, note, bufLen * 2, flags, nUncompressedSamplesThisIteration);
            }
#endif
        }
#ifndef VERSION_EU
    }

    t9 = bufLen * 2;
    aSetBuffer(cmd++, 0, 0, DMEM_ADDR_TEMP, t9);
    aInterleave(cmd++, DMEM_ADDR_LEFT_CH, DMEM_ADDR_RIGHT_CH);
    t9 *= 2;
    aSetBuffer(cmd++, 0, 0, DMEM_ADDR_TEMP, t9);
    aSaveBuffer(cmd++, VIRTUAL_TO_PHYSICAL2(aiBuf));
#endif

    return cmd;
}

// EU Non-N64 version
#elif VERSION_EU
// Processes just one note, not all
u64 *synthesis_process_note(struct Note *note, struct NoteSubEu *noteSubEu, struct NoteSynthesisState *synthesisState, UNUSED s16 *aiBuf, s32 bufLen, u64 *cmd) {
    UNUSED s32 pad0[3];
    struct AudioBankSample *audioBookSample;
    struct AdpcmLoop *loopInfo;
    s16 *curLoadedBook = NULL;
    UNUSED u8 padEU[0x04];
    UNUSED u8 pad8[0x04];
    s32 noteFinished;
    s32 restart;
    s32 flags;
    u16 resamplingRateFixedPoint;
    UNUSED u8 pad7[0x0c];
    UNUSED s32 tempBufLen;
    s32 samplePosAlignmentOffset;
    UNUSED u32 pad9;
    s32 nAdpcmSamplesProcessed;
    s32 nAdpcmPacketsThisIteration;
    u8 *sampleAddr;
    s32 samplesSkippedThisIteration;

    // sp6c is a temporary!

    s32 samplesLenAdjusted; // 108,      spEC
    // Might have been used to store (samplesLenFixedPoint >> 0x10), but doing so causes strange
    // behavior with the break near the end of the loop, causing US and JP to need a goto instead
    UNUSED s32 samplesLenInt;
    s32 endPos;
    s32 nSamplesToProcess;
    s32 samplePosIntLowerNibble;

    s32 nUncompressedSamplesThisIteration;
    s32 s3;
    s32 decodeTailPtr; // Location to write next sample data

    u32 samplesLenFixedPoint;
    s32 nSamplesInThisIteration;
    u32 sampleDataOffset;
    u8 *sampleDataAddr; // Address where ADPCM data is stored after loaded from cart
    s32 nParts;
    s32 curPart;

    s32 temp;

    s32 decodeTailPtrAligned; // Aligned, made right before decode
    s32 resampledTempLen;
    u16 noteSamplesDmemAddrBeforeResampling;

        if (note->noteSubEu.enabled == FALSE) {
            return cmd;
        } else {
            flags = 0;
            tempBufLen = bufLen;

            if (noteSubEu->needsInit == TRUE) {
                flags = A_INIT;
                synthesisState->restart = FALSE;
                synthesisState->samplePosInt = 0;
                synthesisState->samplePosFrac = 0;
                synthesisState->curVolLeft = 1;
                synthesisState->curVolRight = 1;
                synthesisState->prevHeadsetPanRight = 0;
                synthesisState->prevHeadsetPanLeft = 0;
            }

            resamplingRateFixedPoint = noteSubEu->resamplingRateFixedPoint;
            nParts = noteSubEu->hasTwoAdpcmParts + 1;
            samplesLenFixedPoint = (resamplingRateFixedPoint * tempBufLen * 2) + synthesisState->samplePosFrac;
            synthesisState->samplePosFrac = samplesLenFixedPoint & 0xFFFF;

            if (noteSubEu->isSyntheticWave) {
                cmd = load_wave_samples(cmd, noteSubEu, synthesisState, samplesLenFixedPoint >> 0x10);
                noteSamplesDmemAddrBeforeResampling = (synthesisState->samplePosInt * 2) + DMEM_ADDR_UNCOMPRESSED_NOTE;
                synthesisState->samplePosInt += samplesLenFixedPoint >> 0x10;
            }
            else {
                // ADPCM note

                audioBookSample = noteSubEu->sound.audioBankSound->sample;

                loopInfo = audioBookSample->loop;
                endPos = loopInfo->end;
                sampleAddr = audioBookSample->sampleAddr;
                resampledTempLen = 0;
                for (curPart = 0; curPart < nParts; curPart++) {
                    nAdpcmSamplesProcessed = 0; // s8
                    decodeTailPtr = 0;                     // s4

                    if (nParts == 1) {
                        samplesLenAdjusted = samplesLenFixedPoint >> 0x10;
                    } else if ((samplesLenFixedPoint >> 0x10) & 1) {
                        samplesLenAdjusted = ((samplesLenFixedPoint >> 0x10) & ~1) + (curPart * 2);
                    }
                    else {
                        samplesLenAdjusted = (samplesLenFixedPoint >> 0x10);
                    }

                    if (curLoadedBook != audioBookSample->book->book) {
                        u32 nEntries; // v1
                        curLoadedBook = audioBookSample->book->book;
                        nEntries = 16 * audioBookSample->book->order * audioBookSample->book->npredictors;
                        aLoadADPCM(cmd++, nEntries, VIRTUAL_TO_PHYSICAL2(curLoadedBook + noteSubEu->bookOffset));
                    }

                    if (noteSubEu->bookOffset) {
                        curLoadedBook = (s16 *) &euUnknownData_80301950; // what's this? never read
                    }

                    while (nAdpcmSamplesProcessed != samplesLenAdjusted) {
                        s32 samplesRemaining; // v1
                        s32 nUncompressedSamplesThisIteration;

                        noteFinished = FALSE;
                        restart = FALSE;
                        nSamplesToProcess = samplesLenAdjusted - nAdpcmSamplesProcessed;
                        samplePosIntLowerNibble = synthesisState->samplePosInt & 0xf;
                        samplesRemaining = endPos - synthesisState->samplePosInt;

                        if (samplePosIntLowerNibble == 0 && synthesisState->restart == FALSE) {
                            samplePosIntLowerNibble = 16;
                        }
                        samplesSkippedThisIteration = 16 - samplePosIntLowerNibble; // a1

                        if (nSamplesToProcess < samplesRemaining) {
                            nAdpcmPacketsThisIteration = (nSamplesToProcess - samplesSkippedThisIteration + 0xf) / 16;
                            nUncompressedSamplesThisIteration = nAdpcmPacketsThisIteration * 16;
                            s3 = samplesSkippedThisIteration + nUncompressedSamplesThisIteration - nSamplesToProcess;
                        } else {
                            nUncompressedSamplesThisIteration = samplesRemaining - samplesSkippedThisIteration;
                            s3 = 0;
                            if (nUncompressedSamplesThisIteration <= 0) {
                                nUncompressedSamplesThisIteration = 0;
                                samplesSkippedThisIteration = samplesRemaining;
                            }
                            nAdpcmPacketsThisIteration = (nUncompressedSamplesThisIteration + 0xf) / 16;
                            if (loopInfo->count != 0) {
                                // Loop around and restart
                                restart = 1;
                            } else {
                                noteFinished = 1;
                            }
                        }

                        if (nAdpcmPacketsThisIteration != 0) {
                            temp = (synthesisState->samplePosInt - samplePosIntLowerNibble + 0x10) / 16;
                            if (audioBookSample->loaded == 0x81) {
                                sampleDataAddr = sampleAddr + temp * 9;
                            } else {
                                sampleDataAddr = dma_sample_data(
                                    (uintptr_t) (sampleAddr + temp * 9),
                                    nAdpcmPacketsThisIteration * 9, flags, &synthesisState->sampleDmaIndex);
                            }
                            sampleDataOffset = (u32)((uintptr_t) sampleDataAddr & 0xf);
                            aSetBuffer(cmd++, 0, DMEM_ADDR_COMPRESSED_ADPCM_DATA, 0, nAdpcmPacketsThisIteration * 9 + sampleDataOffset);
                            aLoadBuffer(cmd++, VIRTUAL_TO_PHYSICAL2(sampleDataAddr - sampleDataOffset));
                        } else {
                            nUncompressedSamplesThisIteration = 0;
                            sampleDataOffset = 0;
                        }

                        if (synthesisState->restart != FALSE) {
                            aSetLoop(cmd++, VIRTUAL_TO_PHYSICAL2(audioBookSample->loop->state));
                            flags = A_LOOP; // = 2
                            synthesisState->restart = FALSE;
                        }

                        nSamplesInThisIteration = nUncompressedSamplesThisIteration + samplesSkippedThisIteration - s3;
                        if (nAdpcmSamplesProcessed == 0) {
                            aSetBuffer(cmd++, 0, DMEM_ADDR_COMPRESSED_ADPCM_DATA + sampleDataOffset,
                                       DMEM_ADDR_UNCOMPRESSED_NOTE, nUncompressedSamplesThisIteration * 2);
                            aADPCMdec(cmd++, flags,
                                      VIRTUAL_TO_PHYSICAL2(synthesisState->synthesisBuffers->adpcmdecState));
                            samplePosAlignmentOffset = samplePosIntLowerNibble * 2;
                        } else {
                            decodeTailPtrAligned = ALIGN(decodeTailPtr, 5);
                            aSetBuffer(cmd++, 0, DMEM_ADDR_COMPRESSED_ADPCM_DATA + sampleDataOffset,
                                       DMEM_ADDR_UNCOMPRESSED_NOTE + decodeTailPtrAligned, nUncompressedSamplesThisIteration * 2);
                            aADPCMdec(cmd++, flags,
                                      VIRTUAL_TO_PHYSICAL2(synthesisState->synthesisBuffers->adpcmdecState));
                            aDMEMMove(cmd++, DMEM_ADDR_UNCOMPRESSED_NOTE + decodeTailPtrAligned + (samplePosIntLowerNibble * 2),
                                      DMEM_ADDR_UNCOMPRESSED_NOTE + decodeTailPtr, (nSamplesInThisIteration) * 2);
                        }

                        nAdpcmSamplesProcessed += nSamplesInThisIteration;

                        switch (flags) {
                            case A_INIT: // = 1
                                samplePosAlignmentOffset = 0;
                                decodeTailPtr = nUncompressedSamplesThisIteration * 2 + decodeTailPtr;
                                break;

                            case A_LOOP: // = 2
                                decodeTailPtr = nSamplesInThisIteration * 2 + decodeTailPtr;
                                break;

                            default:
                                if (decodeTailPtr != 0) {
                                    decodeTailPtr = nSamplesInThisIteration * 2 + decodeTailPtr;
                                } else {
                                    decodeTailPtr = (samplePosIntLowerNibble + nSamplesInThisIteration) * 2;
                                }
                                break;
                        }
                        flags = 0;

                        if (noteFinished) {
                            aClearBuffer(cmd++, DMEM_ADDR_UNCOMPRESSED_NOTE + decodeTailPtr,
                                         (samplesLenAdjusted - nAdpcmSamplesProcessed) * 2);
                            noteSubEu->finished = 1;
                            note->noteSubEu.finished = 1;
                            note->noteSubEu.enabled = 0;
                            break;
                        }
                        if (restart) {
                            synthesisState->restart = TRUE;
                            synthesisState->samplePosInt = loopInfo->start;
                        } else {
                            synthesisState->samplePosInt += nSamplesToProcess;
                        }
                    }

                    switch (nParts) {
                        case 1:
                            noteSamplesDmemAddrBeforeResampling = DMEM_ADDR_UNCOMPRESSED_NOTE + samplePosAlignmentOffset;
                            break;

                        case 2:
                            switch (curPart) {
                                case 0:
                                    aSetBuffer(cmd++, 0, DMEM_ADDR_UNCOMPRESSED_NOTE + samplePosAlignmentOffset, DMEM_ADDR_RESAMPLED, samplesLenAdjusted + 4);
                                    aResample(cmd++, A_INIT, 0xff60, VIRTUAL_TO_PHYSICAL2(synthesisState->synthesisBuffers->dummyResampleState));
                                    resampledTempLen = samplesLenAdjusted + 4;
                                    noteSamplesDmemAddrBeforeResampling = DMEM_ADDR_RESAMPLED + 4;
                                    if (noteSubEu->finished != FALSE) {
                                        aClearBuffer(cmd++, DMEM_ADDR_RESAMPLED + resampledTempLen, samplesLenAdjusted + 0x10);
                                    }
                                    break;

                                case 1:
                                    aSetBuffer(cmd++, 0, DMEM_ADDR_UNCOMPRESSED_NOTE + samplePosAlignmentOffset,
                                               DMEM_ADDR_RESAMPLED2,
                                               samplesLenAdjusted + 8);
                                    aResample(cmd++, A_INIT, 0xff60,
                                              VIRTUAL_TO_PHYSICAL2(
                                                  synthesisState->synthesisBuffers->dummyResampleState));
                                    aDMEMMove(cmd++, DMEM_ADDR_RESAMPLED2 + 4,
                                              DMEM_ADDR_RESAMPLED + resampledTempLen,
                                              samplesLenAdjusted + 4);
                                    break;
                            }
                    }

                    if (noteSubEu->finished != FALSE) {
                        break;
                    }
                }
            }

            flags = 0;

            if (noteSubEu->needsInit == TRUE) {
                flags = A_INIT;
                noteSubEu->needsInit = FALSE;
            }

            cmd = final_resample(cmd, synthesisState, bufLen * 2, resamplingRateFixedPoint,
                                 noteSamplesDmemAddrBeforeResampling, flags);

            if (noteSubEu->headsetPanRight != 0 || synthesisState->prevHeadsetPanRight != 0) {
                nUncompressedSamplesThisIteration = 1;
            } else if (noteSubEu->headsetPanLeft != 0 || synthesisState->prevHeadsetPanLeft != 0) {
                nUncompressedSamplesThisIteration = 2;
            } else {
                nUncompressedSamplesThisIteration = 0;
            }

            cmd = process_envelope(cmd, noteSubEu, synthesisState, bufLen, DMEM_ADDR_TEMP, nUncompressedSamplesThisIteration, flags);

            if (noteSubEu->usesHeadsetPanEffects) {
                cmd = note_apply_headset_pan_effects(cmd, noteSubEu, synthesisState, bufLen * 2, flags, nUncompressedSamplesThisIteration);
            }
        }

    return cmd;
}

// US and JP Non-N64 versions
#else

// Cleaned up and somewhat optimized version
u64 *synthesis_process_notes(s16 *aiBuf, s32 bufLen, u64 *cmd) {
    s16* curLoadedBook = NULL;

    for (s32 noteIndex = 0; noteIndex < gMaxSimultaneousNotes; noteIndex++) {
        struct Note* note = &gNotes[noteIndex];

        // If the note is enabled but the audio bank isn't loaded, error.
        //! This function requires note->enabled to be volatile, but it breaks other functions like note_enable.
        //! Casting to a struct with just the volatile bitfield works, but there may be a better way to match.
#ifdef VERSION_US
        if (((struct vNote *)note)->enabled && IS_BANK_LOAD_COMPLETE(note->bankId) == FALSE) {
#else
        if (IS_BANK_LOAD_COMPLETE(note->bankId) == FALSE) {
#endif
            gAudioErrorFlags = (note->bankId << 8) + noteIndex + 0x1000000;
        }
        
        // If the note is loaded and its bank is loaded, play it. Else, continue.
        else if (((struct vNote *)note)->enabled) {
            s32 flags;
            u16 noteSamplesDmemAddrBeforeResampling;

            // Init the note
            if (note->needsInit == TRUE) {
                flags = A_INIT;
                note->samplePosInt = 0;
                note->samplePosFrac = 0;
            } else {
                flags = 0;
            }

            // Clamp the frequency
            if (note->frequency >= US_FLOAT(3.99993))
                note->frequency = US_FLOAT(3.99993);

            // If frequency is >= 2.0, the processing must be split into two parts
            const bool tIsHighFreqNote = note->frequency >= US_FLOAT(2.0);
            const f32 resamplingRate = tIsHighFreqNote ? note->frequency * US_FLOAT(.5) : note->frequency;
            const s32 nParts = ((s32) tIsHighFreqNote) + 1; // If tIsHighFreqNote, 2. Else, 1.

            const u16 resamplingRateFixedPoint = (u16)(s32)(resamplingRate * 32768.0f);
            const u32 samplesLenFixedPoint = note->samplePosFrac + (resamplingRateFixedPoint * bufLen) * 2;
            note->samplePosFrac = samplesLenFixedPoint & 0xFFFF; // 16-bit store, can't reuse

            // A wave synthesis note (not ADPCM)
            if (note->sound == NULL) {
                cmd = load_wave_samples(cmd, note, samplesLenFixedPoint >> 0x10);
                noteSamplesDmemAddrBeforeResampling = DMEM_ADDR_UNCOMPRESSED_NOTE + note->samplePosInt * 2;
                note->samplePosInt += (samplesLenFixedPoint >> 0x10);
                flags = 0;
            }

            // An ADPCM note
            else {
                const struct AudioBankSample *audioBookSample = note->sound->sample;
                const struct AdpcmLoop *loopInfo = audioBookSample->loop;
                u8* sampleAddr = audioBookSample->sampleAddr;

                s32 resampledTempLen = 0;

                // If the wrong ADPCM book is loaded, load the right one
                if (curLoadedBook != audioBookSample->book->book) {
                    const u32 nEntries = audioBookSample->book->order * audioBookSample->book->npredictors;
                    curLoadedBook = audioBookSample->book->book;
                    aLoadADPCM(cmd++, nEntries * 16, VIRTUAL_TO_PHYSICAL2(curLoadedBook));
                }

                // Execute 1 or 2 times depending on frequency
                for (s32 curPart = 0; curPart < nParts; curPart++) {
                    s32 nAdpcmSamplesProcessed = 0;
                    s32 decodeTailPtr = 0;  // Points to the first free byte in the uncompressed note buffer
                    s32 samplesLenAdjusted;
                    s32 samplePosAlignmentOffset; // Offset into the ADPCM output buffer start pos

                    // Adjust sample length if we have two parts
                    if (nParts == 1) {
                        samplesLenAdjusted = samplesLenFixedPoint >> 0x10;
                    } else if ((samplesLenFixedPoint >> 0x10) & 1) {
                        samplesLenAdjusted = ((samplesLenFixedPoint >> 0x10) & ~1) + (curPart * 2);
                    }
                    else {
                        samplesLenAdjusted = (samplesLenFixedPoint >> 0x10);
                    }

                    /*
                     * Process every sample in this part of the note
                     * Contents run gMaxSimultaneousNotes * (parts per note (2 max)) * (samplesLenAdjusted per note)
                     * Important information:
                     *   ADPCM is compressed data, stored in chunks of 9 bytes.
                     *   PCM is uncompressed data, stored in 16-bit unsigned ints.
                     *   For every 16 PCM samples, 9 bytes of ADPCM + the previous 2 bytes of PCM are used.
                     *   A Sample Chunk is the smallest amount of data that adpcmDec decodes at once
                    */
                    
                    while (nAdpcmSamplesProcessed != samplesLenAdjusted) {
                        const s32 samplesRemaining = loopInfo->end - note->samplePosInt; // samples until the end of this part of the note
                        const s32 nSamplesToProcess = samplesLenAdjusted - nAdpcmSamplesProcessed; // samples to process this notePart
                        s32 samplePosIntLowerNibble = note->samplePosInt & 15; // Aligned to 16-byte chunks
                        s32 nAdpcmPacketsThisIteration; // Data is decoded one packet at a time (16 samples PCM from 9 bytes ADPCM)
                        s32 nUncompressedSamplesThisIteration; // 
                        bool noteFinished = false, restart = false;

                        // If we can avoid skipping samples, do so
                        if (samplePosIntLowerNibble == 0 && note->restart == FALSE) {
                            samplePosIntLowerNibble = 16;
                        }

                        s32 samplesSkippedThisIteration = 16 - samplePosIntLowerNibble; // Alignment
                        s32 s3;

                        // If we have more chunks after this one, process a full chunk
                        if (nSamplesToProcess < samplesRemaining) {
                            nAdpcmPacketsThisIteration = (nSamplesToProcess - samplesSkippedThisIteration + 15) / 16;
                            nUncompressedSamplesThisIteration = nAdpcmPacketsThisIteration * 16;
                            s3 = samplesSkippedThisIteration + nUncompressedSamplesThisIteration - nSamplesToProcess;
                        }

                        // The final sample chunk, which may be smaller than the rest
                        else {
                            nUncompressedSamplesThisIteration = samplesRemaining + samplePosIntLowerNibble - 16;
                            s3 = 0;
                            if (nUncompressedSamplesThisIteration <= 0) {
                                nUncompressedSamplesThisIteration = 0;
                                samplesSkippedThisIteration = samplesRemaining;
                            }
                            nAdpcmPacketsThisIteration = (nUncompressedSamplesThisIteration + 15) / 16;
                            
                            if (loopInfo->count != 0)
                                restart =  true;
                            else
                                noteFinished = true;
                        }


#ifdef ENHANCED_RSPA_EMULATION

                        uint8_t* directSampleAddr = 0; // Sample data is read directly from emuROM

                        if (nAdpcmPacketsThisIteration == 0)
                            nUncompressedSamplesThisIteration = 0;
                        else
                            directSampleAddr = sampleAddr + (((note->samplePosInt - samplePosIntLowerNibble + 16) >> 4) * 9);

#else

                        u32 sampleDataOffset; // Aligns sample data to 16-byte chunks.

                        // Load compressed ADPCM data into the RSP
                        if (nAdpcmPacketsThisIteration != 0) {
                            const s32 tempNumSamples = (note->samplePosInt - samplePosIntLowerNibble + 16) / 16;
                            
                            // Load sample data
                            const u8* sampleDataAddr = dma_sample_data(
                                (uintptr_t) (sampleAddr + tempNumSamples * 9), // Source addr
                                nAdpcmPacketsThisIteration * 9, // size
                                flags,
                                &note->sampleDmaIndex);

                            sampleDataOffset = (u32)((uintptr_t) sampleDataAddr & 0b1111); // lower 4 bits of address (aligned down to 16) 

                            // Load ADPCM data into DMEM_ADDR_COMPRESSED_ADPCM_DATA
                            aSetBuffer(cmd++, 0, DMEM_ADDR_COMPRESSED_ADPCM_DATA, 0, nAdpcmPacketsThisIteration * 9 + sampleDataOffset); 
                            aLoadBuffer(cmd++, VIRTUAL_TO_PHYSICAL2(sampleDataAddr - sampleDataOffset)); 
                        } else {
                            nUncompressedSamplesThisIteration = 0;
                            sampleDataOffset = 0;
                        }
#endif

                        // If we need to restart the note, loop it
                        if (note->restart != FALSE) {
                            aSetLoop(cmd++, VIRTUAL_TO_PHYSICAL2(audioBookSample->loop->state));
                            flags = A_LOOP; // = 2
                            note->restart = FALSE;
                        }

                        const s32 nSamplesInThisIteration = nUncompressedSamplesThisIteration + samplesSkippedThisIteration - s3;

#ifdef ENHANCED_RSPA_EMULATION
                        
                        // Decode some data
                        // If this is the firt decode, do an unaligned chunk to get us aligned to 32-byte chunks.
                        if (nAdpcmSamplesProcessed == 0) {
                            aSetBuffer(cmd++, 0, /*Unused IN*/ 0, DMEM_ADDR_UNCOMPRESSED_NOTE, nUncompressedSamplesThisIteration * 2);
                            aADPCMdecDirect(cmd++, flags, VIRTUAL_TO_PHYSICAL2(note->synthesisBuffers->adpcmdecState), directSampleAddr);
                            samplePosAlignmentOffset = samplePosIntLowerNibble * 2;
                        }
                        
                        // If this is not the first decode, decode aligned to 32-byte chunks
                        // and then the data is copied to unaligned memory
                        else {
                            const s32 alignedDecodeAddr = ALIGN(decodeTailPtr, 5);
                            aSetBuffer(cmd++, 0, /*Unused IN*/ 0, DMEM_ADDR_UNCOMPRESSED_NOTE + alignedDecodeAddr, nUncompressedSamplesThisIteration * 2);

                            aADPCMdecDirect(cmd++, flags, VIRTUAL_TO_PHYSICAL2(note->synthesisBuffers->adpcmdecState), directSampleAddr);

                            // Shift our aligned data down to the unaligned destination
                            aDMEMMove(
                                cmd++,
                                DMEM_ADDR_UNCOMPRESSED_NOTE + alignedDecodeAddr + (samplePosIntLowerNibble * 2), // input
                                DMEM_ADDR_UNCOMPRESSED_NOTE + decodeTailPtr, // output
                                nSamplesInThisIteration * 2); // nbytes
                        }
#else
                        // Decode some data
                        // If this is the firt decode, do an unaligned chunk to get us aligned to 32-byte chunks.
                        if (nAdpcmSamplesProcessed == 0) {
                            aSetBuffer(cmd++, 0, DMEM_ADDR_COMPRESSED_ADPCM_DATA + sampleDataOffset, DMEM_ADDR_UNCOMPRESSED_NOTE, nUncompressedSamplesThisIteration * 2);
                            aADPCMdec(cmd++, flags, VIRTUAL_TO_PHYSICAL2(note->synthesisBuffers->adpcmdecState));
                            samplePosAlignmentOffset = samplePosIntLowerNibble * 2;
                        }

                        
                        
                        // If this is not the first decode, decode aligned to 32-byte chunks
                        // and then the data is copied to unaligned memory
                        else {
                            const s32 alignedDecodeAddr = ALIGN(decodeTailPtr, 5);
                            aSetBuffer(cmd++, 0, DMEM_ADDR_COMPRESSED_ADPCM_DATA + sampleDataOffset, DMEM_ADDR_UNCOMPRESSED_NOTE + alignedDecodeAddr, nUncompressedSamplesThisIteration * 2);
                            aADPCMdec(cmd++, flags, VIRTUAL_TO_PHYSICAL2(note->synthesisBuffers->adpcmdecState));

                            // Shift our aligned data down to the unaligned destination
                            aDMEMMove(
                                cmd++,
                                DMEM_ADDR_UNCOMPRESSED_NOTE + alignedDecodeAddr + (samplePosIntLowerNibble * 2), // input
                                DMEM_ADDR_UNCOMPRESSED_NOTE + decodeTailPtr, // output
                                nSamplesInThisIteration * 2); // nbytes
                        }
#endif

                        nAdpcmSamplesProcessed += nSamplesInThisIteration;

                        switch (flags) {
                            case A_INIT: // = 1
                                samplePosAlignmentOffset = 0;
                                decodeTailPtr = nUncompressedSamplesThisIteration * 2 + decodeTailPtr;
                                break;

                            case A_LOOP: // = 2
                                decodeTailPtr = nSamplesInThisIteration * 2 + decodeTailPtr;
                                break;

                            default:
                                if (decodeTailPtr != 0) {
                                    decodeTailPtr = nSamplesInThisIteration * 2 + decodeTailPtr;
                                } else {
                                    decodeTailPtr = (samplePosIntLowerNibble + nSamplesInThisIteration) * 2;
                                }
                                break;
                        }
                        flags = 0;

                        // If the note is finished, clear junk data from the end of the buffer,
                        // disable the note, and exit the loop.
                        if (noteFinished) {
                            aClearBuffer(cmd++, DMEM_ADDR_UNCOMPRESSED_NOTE + decodeTailPtr,
                                         (samplesLenAdjusted - nAdpcmSamplesProcessed) * 2);
                            note->samplePosInt = 0;
                            note->finished = 1;
                            ((struct vNote *)note)->enabled = 0;
                            break;
                        }

                        else if (restart) {
                            note->restart = TRUE;
                            note->samplePosInt = loopInfo->start;
                        }
                        
                        else {
                            note->samplePosInt += nSamplesToProcess;
                        }
                    }

                    // End of while-loop synthesis
                    // ADPCM only

                    switch (nParts) {

                        // If we have one part, don't resample.
                        case 1:
                            noteSamplesDmemAddrBeforeResampling = DMEM_ADDR_UNCOMPRESSED_NOTE + samplePosAlignmentOffset;
                            break;

                        // If we have two parts (high-pitched notes), resample
                        case 2:
                            switch (curPart) {
                                case 0:
                                    aSetBuffer(cmd++, 0, DMEM_ADDR_UNCOMPRESSED_NOTE + samplePosAlignmentOffset, DMEM_ADDR_RESAMPLED, samplesLenAdjusted + 4);
                                    aResample(cmd++, A_INIT, 0xff60, VIRTUAL_TO_PHYSICAL2(note->synthesisBuffers->dummyResampleState));
                                    resampledTempLen = samplesLenAdjusted + 4;
                                    noteSamplesDmemAddrBeforeResampling = DMEM_ADDR_RESAMPLED + 4;
                                    if (note->finished != FALSE) {
                                        aClearBuffer(cmd++, DMEM_ADDR_RESAMPLED + resampledTempLen, samplesLenAdjusted + 0x10);
                                    }
                                    break;

                                case 1:
                                    aSetBuffer(cmd++, 0, DMEM_ADDR_UNCOMPRESSED_NOTE + samplePosAlignmentOffset,
                                               DMEM_ADDR_RESAMPLED2,
                                               samplesLenAdjusted + 8);
                                    aResample(cmd++, A_INIT, 0xff60,
                                              VIRTUAL_TO_PHYSICAL2(
                                                  note->synthesisBuffers->dummyResampleState));
                                    aDMEMMove(cmd++, DMEM_ADDR_RESAMPLED2 + 4,
                                              DMEM_ADDR_RESAMPLED + resampledTempLen,
                                              samplesLenAdjusted + 4);
                                    break;
                            }
                    }

                    if (note->finished != FALSE) {
                        break;
                    }

                } // end for (curPart = 0; curPart < nParts; curPart++){...}
            } // End if(...) {synthetic} else {adpcm}

            // Our note is now fully decompressed/synthesized.
            // Do one final resample, process envelope, and apply headset panning.

            flags = 0;

            if (note->needsInit == TRUE) {
                flags = A_INIT;
                note->needsInit = FALSE;
            }

            cmd = final_resample(cmd, note, bufLen * 2, resamplingRateFixedPoint,
                                 noteSamplesDmemAddrBeforeResampling, flags);

            // panRight = 1, panLeft = 2, else 0
            const u16 panRight = note->headsetPanRight | note->prevHeadsetPanRight;
            const u16 panLeft = note->headsetPanLeft | note->prevHeadsetPanLeft;
            const s32 panSettings = panRight ? 1 : (panLeft ? 2 : 0);

            // Stereo panning is handled here
            cmd = process_envelope(cmd, note, bufLen, DMEM_ADDR_TEMP, panSettings, flags);

            // Only ever set if gSoundMode == HEADSET. Applies extra panning nonsense.
            if (note->usesHeadsetPanEffects) {
                cmd = note_apply_headset_pan_effects(cmd, note, bufLen * 2, flags, panSettings);
            }

        } // end of if(data is not yet loaded) {skip note} else {process note}
    } // end of for(each note) {process}

    // All notes are now processed. Interleave the audio and save it.



// If possible, interleave directly into the output buf and skip the discrete copy.
#ifdef ENHANCED_RSPA_EMULATION
    const s32 outputBufLen = bufLen * 2;
    aSetBuffer(cmd++, 0, 0, /*Unused OUT*/ 0, outputBufLen);

    // Avoid redundant copy on 3DS when possible
    DO_3DS(
        if (audio_3ds_next_buffer_is_ready())
            aiBuf = direct_buf + (aiBuf - sCurAiBufBasePtr);
        else
            samples_to_copy += bufLen;
    );

    aInterleaveAndCopy(cmd++, DMEM_ADDR_LEFT_CH, DMEM_ADDR_RIGHT_CH, aiBuf);

// Else if enhanced RSPA Emulation is disabled, move to the copy buf.
#else
    const s32 outputBufLen = bufLen * 2;
    aSetBuffer(cmd++, 0, 0, DMEM_ADDR_TEMP, outputBufLen);
    aInterleave(cmd++, DMEM_ADDR_LEFT_CH, DMEM_ADDR_RIGHT_CH);

    aSetBuffer(cmd++, 0, 0, DMEM_ADDR_TEMP, outputBufLen * 2);

    // Avoid redundant copy on 3DS when possible
    DO_3DS(
        if (audio_3ds_next_buffer_is_ready())
            aiBuf = direct_buf + (aiBuf - sCurAiBufBasePtr);
        else
            samples_to_copy += bufLen;
    );

    aSaveBuffer(cmd++, VIRTUAL_TO_PHYSICAL2(aiBuf));
#endif

    return cmd;
}

#endif

#ifdef VERSION_EU
u64 *load_wave_samples(u64 *cmd, struct NoteSubEu *noteSubEu, struct NoteSynthesisState *synthesisState, s32 nSamplesToLoad) {
    s32 a3;
    s32 i;
    s32 repeats;
    aSetBuffer(cmd++, /*flags*/ 0, /*dmemin*/ DMEM_ADDR_UNCOMPRESSED_NOTE, /*dmemout*/ 0, /*count*/ 128);
    aLoadBuffer(cmd++, VIRTUAL_TO_PHYSICAL2(noteSubEu->sound.samples));
    synthesisState->samplePosInt &= 0x3f;
    a3 = 64 - synthesisState->samplePosInt;
    if (a3 < nSamplesToLoad) {
        repeats = (nSamplesToLoad - a3 + 63) / 64;
        for (i = 0; i < repeats; i++) {
            aDMEMMove(cmd++,
                      /*dmemin*/ DMEM_ADDR_UNCOMPRESSED_NOTE,
                      /*dmemout*/ DMEM_ADDR_UNCOMPRESSED_NOTE + (1 + i) * 128,
                      /*count*/ 128);
        }
    }
    return cmd;
}
#else
u64 *load_wave_samples(u64 *cmd, struct Note *note, s32 nSamplesToLoad) {
    s32 a3;
    s32 i;
    aSetBuffer(cmd++, /*flags*/ 0, /*dmemin*/ DMEM_ADDR_UNCOMPRESSED_NOTE, /*dmemout*/ 0,
               /*count*/ sizeof(note->synthesisBuffers->samples));
    aLoadBuffer(cmd++, VIRTUAL_TO_PHYSICAL2(note->synthesisBuffers->samples));
    note->samplePosInt &= (note->sampleCount - 1);
    a3 = 64 - note->samplePosInt;
    if (a3 < nSamplesToLoad) {
        for (i = 0; i <= (nSamplesToLoad - a3 + 63) / 64 - 1; i++) {
            aDMEMMove(cmd++, /*dmemin*/ DMEM_ADDR_UNCOMPRESSED_NOTE, /*dmemout*/ DMEM_ADDR_UNCOMPRESSED_NOTE + (1 + i) * sizeof(note->synthesisBuffers->samples), /*count*/ sizeof(note->synthesisBuffers->samples));
        }
    }
    return cmd;
}
#endif

#ifdef VERSION_EU
u64 *final_resample(u64 *cmd, struct NoteSynthesisState *synthesisState, s32 count, u16 pitch, u16 dmemIn, u32 flags) {
    aSetBuffer(cmd++, /*flags*/ 0, dmemIn, /*dmemout*/ 0, count);
    aResample(cmd++, flags, pitch, VIRTUAL_TO_PHYSICAL2(synthesisState->synthesisBuffers->finalResampleState));
    return cmd;
}
#else
u64 *final_resample(u64 *cmd, struct Note *note, s32 count, u16 pitch, u16 dmemIn, u32 flags) {
    aSetBuffer(cmd++, /*flags*/ 0, dmemIn, /*out*/ DMEM_ADDR_TEMP, count);
    aResample(cmd++, flags, pitch, VIRTUAL_TO_PHYSICAL2(note->synthesisBuffers->finalResampleState));
    return cmd;
}
#endif

#ifndef VERSION_EU
u64 *process_envelope(u64 *cmd, struct Note *note, s32 nSamples, u16 inBuf, s32 headsetPanSettings,
                      UNUSED u32 flags) {
    UNUSED u8 pad[16];
    struct VolumeChange vol;
    vol.sourceLeft = note->curVolLeft;
    vol.sourceRight = note->curVolRight;
    vol.targetLeft = note->targetVolLeft;
    vol.targetRight = note->targetVolRight;
    note->curVolLeft = vol.targetLeft;
    note->curVolRight = vol.targetRight;
    return process_envelope_inner(cmd, note, nSamples, inBuf, headsetPanSettings, &vol);
}

u64 *process_envelope_inner(u64 *cmd, struct Note *note, s32 nSamples, u16 inBuf,
                            s32 headsetPanSettings, struct VolumeChange *vol) {
    UNUSED u8 pad[3];
    u8 mixerFlags;
    UNUSED u8 pad2[8];
    s32 rampLeft, rampRight;
#else
u64 *process_envelope(u64 *cmd, struct NoteSubEu *note, struct NoteSynthesisState *synthesisState, s32 nSamples, u16 inBuf, s32 headsetPanSettings, UNUSED u32 flags) {
    UNUSED u8 pad1[20];
    u16 sourceRight;
    u16 sourceLeft;
    UNUSED u8 pad2[4];
    u16 targetLeft;
    u16 targetRight;
    s32 mixerFlags;
    s32 rampLeft;
    s32 rampRight;

    sourceLeft = synthesisState->curVolLeft;
    sourceRight = synthesisState->curVolRight;
    targetLeft = (note->targetVolLeft << 5);
    targetRight = (note->targetVolRight << 5);
    if (targetLeft == 0) {
        targetLeft++;
    }
    if (targetRight == 0) {
        targetRight++;
    }
    synthesisState->curVolLeft = targetLeft;
    synthesisState->curVolRight = targetRight;
#endif

    // For aEnvMixer, five buffers and count are set using aSetBuffer.
    // in, dry left, count without A_AUX flag.
    // dry right, wet left, wet right with A_AUX flag.

    if (note->usesHeadsetPanEffects) {
        aClearBuffer(cmd++, DMEM_ADDR_NOTE_PAN_TEMP, DEFAULT_LEN_1CH);

        switch (headsetPanSettings) {
            case 1:
                aSetBuffer(cmd++, 0, inBuf, DMEM_ADDR_NOTE_PAN_TEMP, nSamples * 2);
                aSetBuffer(cmd++, A_AUX, DMEM_ADDR_RIGHT_CH, DMEM_ADDR_WET_LEFT_CH,
                           DMEM_ADDR_WET_RIGHT_CH);
                break;
            case 2:
                aSetBuffer(cmd++, 0, inBuf, DMEM_ADDR_LEFT_CH, nSamples * 2);
                aSetBuffer(cmd++, A_AUX, DMEM_ADDR_NOTE_PAN_TEMP, DMEM_ADDR_WET_LEFT_CH,
                           DMEM_ADDR_WET_RIGHT_CH);
                break;
            default:
                aSetBuffer(cmd++, 0, inBuf, DMEM_ADDR_LEFT_CH, nSamples * 2);
                aSetBuffer(cmd++, A_AUX, DMEM_ADDR_RIGHT_CH, DMEM_ADDR_WET_LEFT_CH,
                           DMEM_ADDR_WET_RIGHT_CH);
                break;
        }
    } else {
        // It's a bit unclear what the "stereo strong" concept does.
        // Instead of mixing the opposite channel to the normal buffers, the sound is first
        // mixed into a temporary buffer and then subtracted from the normal buffer.
        if (note->stereoStrongRight) {
            aClearBuffer(cmd++, DMEM_ADDR_STEREO_STRONG_TEMP_DRY, DEFAULT_LEN_2CH);
            aSetBuffer(cmd++, 0, inBuf, DMEM_ADDR_STEREO_STRONG_TEMP_DRY, nSamples * 2);
            aSetBuffer(cmd++, A_AUX, DMEM_ADDR_RIGHT_CH, DMEM_ADDR_STEREO_STRONG_TEMP_WET,
                       DMEM_ADDR_WET_RIGHT_CH);
        } else if (note->stereoStrongLeft) {
            aClearBuffer(cmd++, DMEM_ADDR_STEREO_STRONG_TEMP_DRY, DEFAULT_LEN_2CH);
            aSetBuffer(cmd++, 0, inBuf, DMEM_ADDR_LEFT_CH, nSamples * 2);
            aSetBuffer(cmd++, A_AUX, DMEM_ADDR_STEREO_STRONG_TEMP_DRY, DMEM_ADDR_WET_LEFT_CH,
                       DMEM_ADDR_STEREO_STRONG_TEMP_WET);
        } else {
            aSetBuffer(cmd++, 0, inBuf, DMEM_ADDR_LEFT_CH, nSamples * 2);
            aSetBuffer(cmd++, A_AUX, DMEM_ADDR_RIGHT_CH, DMEM_ADDR_WET_LEFT_CH, DMEM_ADDR_WET_RIGHT_CH);
        }
    }

#ifdef VERSION_EU
    if (targetLeft == sourceLeft && targetRight == sourceRight && !note->envMixerNeedsInit) {
#else
    if (vol->targetLeft == vol->sourceLeft && vol->targetRight == vol->sourceRight
        && !note->envMixerNeedsInit) {
#endif
        mixerFlags = A_CONTINUE;
    } else {
        mixerFlags = A_INIT;

#ifdef VERSION_EU
        rampLeft = gCurrentLeftVolRamping[targetLeft >> 5] * gCurrentRightVolRamping[sourceLeft >> 5];
        rampRight = gCurrentLeftVolRamping[targetRight >> 5] * gCurrentRightVolRamping[sourceRight >> 5];
#else
        rampLeft = get_volume_ramping(vol->sourceLeft, vol->targetLeft, nSamples);
        rampRight = get_volume_ramping(vol->sourceRight, vol->targetRight, nSamples);
#endif

        // The operation's parameters change meanings depending on flags
#ifdef VERSION_EU
        aSetVolume(cmd++, A_VOL | A_LEFT, sourceLeft, 0, 0);
        aSetVolume(cmd++, A_VOL | A_RIGHT, sourceRight, 0, 0);
        aSetVolume32(cmd++, A_RATE | A_LEFT, targetLeft, rampLeft);
        aSetVolume32(cmd++, A_RATE | A_RIGHT, targetRight, rampRight);
        aSetVolume(cmd++, A_AUX, gVolume, 0, note->reverbVol << 8);
#else
        aSetVolume(cmd++, A_VOL | A_LEFT, vol->sourceLeft, 0, 0);
        aSetVolume(cmd++, A_VOL | A_RIGHT, vol->sourceRight, 0, 0);
        aSetVolume32(cmd++, A_RATE | A_LEFT, vol->targetLeft, rampLeft);
        aSetVolume32(cmd++, A_RATE | A_RIGHT, vol->targetRight, rampRight);
        aSetVolume(cmd++, A_AUX, gVolume, 0, note->reverbVol);
#endif
    }

#ifdef VERSION_EU
    if (gUseReverb && note->reverbVol != 0) {
        aEnvMixer(cmd++, mixerFlags | A_AUX,
                  VIRTUAL_TO_PHYSICAL2(synthesisState->synthesisBuffers->mixEnvelopeState));
#else
    if (gSynthesisReverb.useReverb && note->reverb) {
        aEnvMixer(cmd++, mixerFlags | A_AUX,
                  VIRTUAL_TO_PHYSICAL2(note->synthesisBuffers->mixEnvelopeState));
#endif
        if (note->stereoStrongRight) {
            aSetBuffer(cmd++, 0, 0, 0, nSamples * 2);
            // 0x8000 is -100%, so subtract sound instead of adding...
            aMix(cmd++, 0, /*gain*/ 0x8000, /*in*/ DMEM_ADDR_STEREO_STRONG_TEMP_DRY,
                 /*out*/ DMEM_ADDR_LEFT_CH);
            aMix(cmd++, 0, /*gain*/ 0x8000, /*in*/ DMEM_ADDR_STEREO_STRONG_TEMP_WET,
                 /*out*/ DMEM_ADDR_WET_LEFT_CH);
        } else if (note->stereoStrongLeft) {
            aSetBuffer(cmd++, 0, 0, 0, nSamples * 2);
            aMix(cmd++, 0, /*gain*/ 0x8000, /*in*/ DMEM_ADDR_STEREO_STRONG_TEMP_DRY,
                 /*out*/ DMEM_ADDR_RIGHT_CH);
            aMix(cmd++, 0, /*gain*/ 0x8000, /*in*/ DMEM_ADDR_STEREO_STRONG_TEMP_WET,
                 /*out*/ DMEM_ADDR_WET_RIGHT_CH);
        }
    } else {
#ifdef VERSION_EU
        aEnvMixer(cmd++, mixerFlags, VIRTUAL_TO_PHYSICAL2(synthesisState->synthesisBuffers->mixEnvelopeState));
#else
        aEnvMixer(cmd++, mixerFlags, VIRTUAL_TO_PHYSICAL2(note->synthesisBuffers->mixEnvelopeState));
#endif
        if (note->stereoStrongRight) {
            aSetBuffer(cmd++, 0, 0, 0, nSamples * 2);
            aMix(cmd++, 0, /*gain*/ 0x8000, /*in*/ DMEM_ADDR_STEREO_STRONG_TEMP_DRY,
                 /*out*/ DMEM_ADDR_LEFT_CH);
        } else if (note->stereoStrongLeft) {
            aSetBuffer(cmd++, 0, 0, 0, nSamples * 2);
            aMix(cmd++, 0, /*gain*/ 0x8000, /*in*/ DMEM_ADDR_STEREO_STRONG_TEMP_DRY,
                 /*out*/ DMEM_ADDR_RIGHT_CH);
        }
    }
    return cmd;
}

#ifdef VERSION_EU
u64 *note_apply_headset_pan_effects(u64 *cmd, struct NoteSubEu *noteSubEu, struct NoteSynthesisState *note, s32 bufLen, s32 flags, s32 leftRight) {
#else
u64 *note_apply_headset_pan_effects(u64 *cmd, struct Note *note, s32 bufLen, s32 flags, s32 leftRight) {
#endif
    u16 dest;
    u16 pitch; // t2
#ifndef VERSION_EU
    u16 prevPanShift;
    u16 panShift;
#else
    u8 prevPanShift;
    u8 panShift;
    UNUSED u8 unkDebug;
#endif

    switch (leftRight) {
        case 1:
            dest = DMEM_ADDR_LEFT_CH;
#ifndef VERSION_EU
            panShift = note->headsetPanRight;
#else
            panShift = noteSubEu->headsetPanRight;
#endif
            note->prevHeadsetPanLeft = 0;
            prevPanShift = note->prevHeadsetPanRight;
            note->prevHeadsetPanRight = panShift;
            break;
        case 2:
            dest = DMEM_ADDR_RIGHT_CH;
#ifndef VERSION_EU
            panShift = note->headsetPanLeft;
#else
            panShift = noteSubEu->headsetPanLeft;
#endif
            note->prevHeadsetPanRight = 0;

            prevPanShift = note->prevHeadsetPanLeft;
            note->prevHeadsetPanLeft = panShift;
            break;
        default:
            return cmd;
    }

    if (flags != 1) // A_INIT?
    {
        // Slightly adjust the sample rate in order to fit a change in pan shift
        if (prevPanShift == 0) {
            // Kind of a hack that moves the first samples into the resample state
            aDMEMMove(cmd++, DMEM_ADDR_NOTE_PAN_TEMP, DMEM_ADDR_TEMP, 8);
            aClearBuffer(cmd++, 8, 8); // Set pitch accumulator to 0 in the resample state
            aDMEMMove(cmd++, DMEM_ADDR_NOTE_PAN_TEMP, DMEM_ADDR_TEMP + 0x10,
                      0x10); // No idea, result seems to be overwritten later

            aSetBuffer(cmd++, 0, 0, DMEM_ADDR_TEMP, 32);
            aSaveBuffer(cmd++, VIRTUAL_TO_PHYSICAL2(note->synthesisBuffers->panResampleState));

#ifdef VERSION_EU
            pitch = (bufLen << 0xf) / (bufLen + panShift - prevPanShift + 8);
            if (pitch) {
            }
#else
            pitch = (bufLen << 0xf) / (panShift + bufLen - prevPanShift + 8);
#endif
            aSetBuffer(cmd++, 0, DMEM_ADDR_NOTE_PAN_TEMP + 8, DMEM_ADDR_TEMP, panShift + bufLen - prevPanShift);
            aResample(cmd++, 0, pitch, VIRTUAL_TO_PHYSICAL2(note->synthesisBuffers->panResampleState));
        } else {
            if (panShift == 0) {
                pitch = (bufLen << 0xf) / (bufLen - prevPanShift - 4);
            } else {
                pitch = (bufLen << 0xf) / (bufLen + panShift - prevPanShift);
            }

#if defined(VERSION_EU) && !defined(AVOID_UB)
            if (unkDebug) { // UB
            }
#endif
            aSetBuffer(cmd++, 0, DMEM_ADDR_NOTE_PAN_TEMP, DMEM_ADDR_TEMP, panShift + bufLen - prevPanShift);
            aResample(cmd++, 0, pitch, VIRTUAL_TO_PHYSICAL2(note->synthesisBuffers->panResampleState));
        }

        if (prevPanShift != 0) {
            aSetBuffer(cmd++, 0, DMEM_ADDR_NOTE_PAN_TEMP, 0, prevPanShift);
            aLoadBuffer(cmd++, VIRTUAL_TO_PHYSICAL2(note->synthesisBuffers->panSamplesBuffer));
            aDMEMMove(cmd++, DMEM_ADDR_TEMP, DMEM_ADDR_NOTE_PAN_TEMP + prevPanShift, panShift + bufLen - prevPanShift);
        } else {
            aDMEMMove(cmd++, DMEM_ADDR_TEMP, DMEM_ADDR_NOTE_PAN_TEMP, panShift + bufLen - prevPanShift);
        }
    } else {
        // Just shift right
        aDMEMMove(cmd++, DMEM_ADDR_NOTE_PAN_TEMP, DMEM_ADDR_TEMP, bufLen);
        aDMEMMove(cmd++, DMEM_ADDR_TEMP, DMEM_ADDR_NOTE_PAN_TEMP + panShift, bufLen);
        aClearBuffer(cmd++, DMEM_ADDR_NOTE_PAN_TEMP, panShift);
    }

    if (panShift) {
        // Save excessive samples for next iteration
        aSetBuffer(cmd++, 0, 0, DMEM_ADDR_NOTE_PAN_TEMP + bufLen, panShift);
        aSaveBuffer(cmd++, VIRTUAL_TO_PHYSICAL2(note->synthesisBuffers->panSamplesBuffer));
    }

    aSetBuffer(cmd++, 0, 0, 0, bufLen);
    aMix(cmd++, 0, /*gain*/ 0x7fff, /*in*/ DMEM_ADDR_NOTE_PAN_TEMP, /*out*/ dest);

    return cmd;
}

#if !defined(VERSION_EU)
// Moved to playback.c in EU

void note_init_volume(struct Note *note) {
    note->targetVolLeft = 0;
    note->targetVolRight = 0;
    note->reverb = 0;
    note->reverbVol = 0;
    note->unused2 = 0;
    note->curVolLeft = 1;
    note->curVolRight = 1;
    note->frequency = 0.0f;
}

void note_set_vel_pan_reverb(struct Note *note, f32 velocity, f32 pan, u8 reverb) {
    s32 panIndex;
    f32 volLeft;
    f32 volRight;
#ifdef VERSION_JP
    panIndex = MIN((s32)(pan * 127.5), 127);
#else
    panIndex = (s32)(pan * 127.5f) & 127;
#endif
    if (note->stereoHeadsetEffects && gSoundMode == SOUND_MODE_HEADSET) {
        s8 smallPanIndex;
        s8 temp = (s8)(pan * 10.0f);
        if (temp < 9) {
            smallPanIndex = temp;
        } else {
            smallPanIndex = 9;
        }
        note->headsetPanLeft = gHeadsetPanQuantization[smallPanIndex];
        note->headsetPanRight = gHeadsetPanQuantization[9 - smallPanIndex];
        note->stereoStrongRight = FALSE;
        note->stereoStrongLeft = FALSE;
        note->usesHeadsetPanEffects = TRUE;
        volLeft = gHeadsetPanVolume[panIndex];
        volRight = gHeadsetPanVolume[127 - panIndex];
    } else if (note->stereoHeadsetEffects && gSoundMode == SOUND_MODE_STEREO) {
        u8 strongLeft;
        u8 strongRight;
        strongLeft = FALSE;
        strongRight = FALSE;
        note->headsetPanLeft = 0;
        note->headsetPanRight = 0;
        note->usesHeadsetPanEffects = FALSE;
        volLeft = gStereoPanVolume[panIndex];
        volRight = gStereoPanVolume[127 - panIndex];
        if (panIndex < 0x20) {
            strongLeft = TRUE;
        } else if (panIndex > 0x60) {
            strongRight = TRUE;
        }
        note->stereoStrongRight = strongRight;
        note->stereoStrongLeft = strongLeft;
    } else if (gSoundMode == SOUND_MODE_MONO) {
        volLeft = .707f;
        volRight = .707f;
    } else {
        volLeft = gDefaultPanVolume[panIndex];
        volRight = gDefaultPanVolume[127 - panIndex];
    }

    if (velocity < 0) {
        velocity = 0;
    }
#ifdef VERSION_JP
    note->targetVolLeft = (u16)(velocity * volLeft) & ~0x80FF; // 0x7F00, but that doesn't match
    note->targetVolRight = (u16)(velocity * volRight) & ~0x80FF;
#else
    note->targetVolLeft = (u16)(s32)(velocity * volLeft) & ~0x80FF;
    note->targetVolRight = (u16)(s32)(velocity * volRight) & ~0x80FF;
#endif
    if (note->targetVolLeft == 0) {
        note->targetVolLeft++;
    }
    if (note->targetVolRight == 0) {
        note->targetVolRight++;
    }
    if (note->reverb != reverb) {
        note->reverb = reverb;
        note->reverbVol = reverb << 8;
        note->envMixerNeedsInit = TRUE;
        return;
    }

    if (note->needsInit) {
        note->envMixerNeedsInit = TRUE;
    } else {
        note->envMixerNeedsInit = FALSE;
    }
}

void note_set_frequency(struct Note *note, f32 frequency) {
    note->frequency = frequency;
}

void note_enable(struct Note *note) {
    note->enabled = TRUE;
    note->needsInit = TRUE;
    note->restart = FALSE;
    note->finished = FALSE;
    note->stereoStrongRight = FALSE;
    note->stereoStrongLeft = FALSE;
    note->usesHeadsetPanEffects = FALSE;
    note->headsetPanLeft = 0;
    note->headsetPanRight = 0;
    note->prevHeadsetPanRight = 0;
    note->prevHeadsetPanLeft = 0;
}

void note_disable(struct Note *note) {
    if (note->needsInit == TRUE) {
        note->needsInit = FALSE;
    } else {
        note_set_vel_pan_reverb(note, 0, .5, 0);
    }
    note->priority = NOTE_PRIORITY_DISABLED;
    note->enabled = FALSE;
    note->finished = FALSE;
    note->parentLayer = NO_LAYER;
    note->prevParentLayer = NO_LAYER;
}
#endif
