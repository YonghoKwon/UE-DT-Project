/* Original test-only LZF grammar validator. No third-party source/dependency.
 * Same exact-size, literal, match-length and backward-distance checks as the
 * independent Python decoder; avoids the GIL/copy cost in a 3x20Hz receiver.
 * Build outputs belong under Saved/Reports, never in the source tree.
 */
__declspec(dllexport) int pcd_validate_lzf(const unsigned char* data,
    unsigned long long size, unsigned long long expected)
{
    unsigned long long i = 0, written = 0;
    if (expected > 8u * 1024u * 1024u) return 0;
    while (i < size)
    {
        unsigned int control = data[i++], count;
        if (control < 32)
        {
            count = control + 1;
            if (count > size-i || count > expected-written) return 0;
            i += count;
        }
        else
        {
            unsigned int distance;
            count = control >> 5;
            if (count == 7)
            {
                if (i >= size) return 0;
                count += data[i++];
            }
            if (i >= size) return 0;
            distance = ((control & 31) << 8) + data[i++] + 1;
            count += 2;
            if (distance > written || count > expected-written) return 0;
        }
        written += count;
    }
    return written == expected;
}
