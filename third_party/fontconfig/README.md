# fontconfig orthographies

`fc-lang/` holds the orthography files (`*.orth`) of fontconfig, unmodified, from <https://gitlab.freedesktop.org/fontconfig/fontconfig> at commit `a4e25ec391d417e4bca052fbfa5cd7ce5f7fd39e` (directory `fc-lang/`). `COPYING` is fontconfig's license file from the same commit; each `.orth` file also carries its own notice.

`scripts/gen-orthographies.mjs` compiles these files into the addon's orthography table. To move to another fontconfig commit, replace the files here with that commit's, update the commit above and regenerate.
