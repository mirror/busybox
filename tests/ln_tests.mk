
# GNU `ln'
GLN = /bin/ln
# BusyBox `ln'
BLN = $(shell pwd)/ln

.PHONY: ln_clean
ln_clean:
	rm -rf ln_tests ln_*.{gnu,bb} ln

.PHONY: ln_tests
ln_tests: ln_clean ln
	@echo;
	@echo "No output from diff means busybox ln is functioning properly.";

	@echo;
	${BLN} || true;

	@echo;
	mkdir ln_tests;

	@echo;
	cd ln_tests;				\
	 echo A file > afile;			\
	 ls -l afile > ../ln_afile_newname.gnu;	\
	 ${GLN} afile newname;			\
	 ls -l afile newname >> ../ln_afile_newname.gnu;

	@echo;
	rm -f ln_tests/{afile,newname};

	@echo;
	cd ln_tests;				\
	 echo A file > afile;			\
	 ls -l afile > ../ln_afile_newname.bb;	\
	 ${BLN} afile newname;			\
	 ls -l afile newname >> ../ln_afile_newname.bb;

	@echo;
	diff -u ln_afile_newname.gnu ln_afile_newname.bb

	@echo;
	rm -f ln_tests/{afile,newname};

	@echo;
	cd ln_tests;				\
	 echo A file > afile;			\
	 ls -l afile > ../ln_s_afile_newname.gnu;	\
	 ${GLN} -s afile newname;		\
	 ls -l afile newname >> ../ln_s_afile_newname.gnu;

	@echo;
	rm -f ln_tests/{afile,newname};

	@echo;
	cd ln_tests;				\
	 echo A file > afile;			\
	 ls -l afile > ../ln_s_afile_newname.bb;	\
	 ${BLN} -s afile newname;		\
	 ls -l afile newname >> ../ln_s_afile_newname.bb;

	@echo;
	diff -u ln_s_afile_newname.gnu ln_s_afile_newname.bb

	@echo;
	rm -f ln_tests/{afile,newname};
