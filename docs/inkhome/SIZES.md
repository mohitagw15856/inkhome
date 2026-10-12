# Size report

App partition: 6,553,600 bytes (0x640000) in Inkhome's own table. Stock
tables: X4 0x640000, X3 0x770000 per slot, so the X4 limit is the one that
matters. Static RAM is out of 327,680 bytes; heap peaks are listed per app
in each module's header comment.

`default` env (development build, debug logging):

| Change | Flash (bytes) | Delta | Static RAM |
|---|---|---|---|
| Upstream 1.6.5, untouched | 5,623,869 | | 57,920 |
| Apps framework and launcher | 5,626,187 | +2,318 | 57,920 |
| If Found | 5,633,841 | +7,654 | 57,936 |
| Switch firmware | 5,638,323 | +4,482 | 57,936 |
| Slot guard and no-SD screen | 5,641,105 | +2,782 | 57,936 |
| InkRecipes | 5,657,185 | +16,080 | 57,936 |
| Review fixes | 5,657,473 | +288 | 57,936 |
| Reading stats | 5,681,487 | +24,014 | 58,048 |
| Custom + stats sleep screen | 5,683,045 | +1,558 | 58,048 |
| Verse (InkVerse) and Verse sleep screen | 5,698,113 | +15,068 | 58,048 |
| Style fixes, branding, .xth pictures, time left, focus timer, owner strip, Cover + stats, Year in books | 5,714,981 | +16,868 | 58,048 |
| Achievements | 5,733,365 | +18,384 | 58,048 |
| Battery health, motion sensor, gestures | 5,743,755 | +10,390 | 58,048 |

`gh_release` env (what users install): **5,610,373 bytes, 85.6%** of the
partition; image file 5,624,448 bytes. Headroom about 929 KB.

Tilt default and OTA changes did not move the size measurably.
