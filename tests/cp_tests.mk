# This is a -*- makefile -*-

# GNU `cp'
GCP = /bin/cp
# BusyBox `cp'
BCP = $(shell pwd)/cp

.PHONY: cp_clean
cp_clean:
	rm -rf cp_tests cp_*.{gnu,bb} cp

.PHONY: cp_tests
cp_tests: cp_clean cp
	@echo;
	@echo "No output from diff means busybox cp is functioning properly.";

	@echo;
	${BCP} || true;

	@echo;
	mkdir cp_tests;

	@echo;
	cd cp_tests;				\
	 echo A file > afile;			\
	 ls -l afile > ../cp_afile_afilecopy.gnu; \
	 ${GCP} afile afilecopy;		\
	 ls -l afile afilecopy >> ../cp_afile_afilecopy.gnu;

	@echo;
	rm -f cp_tests/afile*;

	@echo;
	cd cp_tests;				\
	 echo A file > afile;			\
	 ls -l afile > ../cp_afile_afilecopy.bb; \
	 ${BCP} afile afilecopy;		\
	 ls -l afile afilecopy >> ../cp_afile_afilecopy.bb;

	@echo;
	diff -u cp_afile_afilecopy.gnu cp_afile_afilecopy.bb;

	@echo;
	rm -f cp_tests/afile*;

	@echo; echo;
	cd cp_tests;				\
	 mkdir there there1;			\
	 cd there;				\
	  ln -s ../afile .;

	@echo;
	cd cp_tests;				\
	 echo A file > afile;			\
	 ls -l afile > ../cp_symlink.gnu;	\
	 ${GCP} there/afile there1/;		\
	 ls -l afile there/afile there1/afile >> ../cp_symlink.gnu;

	@echo;
	rm -f cp_tests/afile cp_tests/there1/afile;

	@echo;
	cd cp_tests;				\
	 echo A file > afile;			\
	 ls -l afile > ../cp_symlink.bb;	\
	 ${BCP} there/afile there1/;		\
	 ls -l afile there/afile there1/afile >> ../cp_symlink.bb;

	@echo;
	diff -u cp_symlink.gnu cp_symlink.bb;

	@echo;
	rm -f cp_tests/afile cp_tests/there1/afile;

	@echo; echo;
	cd cp_tests;				\
	 echo A file > afile;			\
	 ls -l afile > ../cp_a_symlink.gnu;	\
	 ${GCP} -a there/afile there1/;		\
	 ls -l afile there/afile there1/afile >> ../cp_a_symlink.gnu;

	@echo;
	rm -f cp_tests/afile cp_tests/there1/afile;

	@echo;
	cd cp_tests;				\
	 echo A file > afile;			\
	 ls -l afile > ../cp_a_symlink.bb;	\
	 ${BCP} -a there/afile there1/;		\
	 ls -l afile there/afile there1/afile >> ../cp_a_symlink.bb;

	@echo;
	diff -u cp_a_symlink.gnu cp_a_symlink.bb;

	@echo;
	rm -f cp_tests/afile
	rm -rf cp_tests/there{,1};

	@echo; echo;
	cd cp_tests;				\
	 echo A file > there/afile;		\
	 mkdir there/adir;			\
	 touch there/adir/afileinadir;		\
	 ln -s $(shell pwd) there/alink;

	@echo;
	cd cp_tests;				\
	 ${GCP} -a there/ there1/;		\
	 ls -lR there/ there1/ > ../cp_a_dir_dir.gnu;

	@echo;
	rm -rf cp_tests/there1;

	@echo;
	cd cp_tests;				\
	 ${BCP} -a there/ there1/;		\
	 ls -lR there/ there1/ > ../cp_a_dir_dir.bb;

	@echo;
	diff -u cp_a_dir_dir.gnu cp_a_dir_dir.bb;

	@echo;
	rm -rf cp_tests/there1/;

	@echo; echo;
	cd cp_tests;				\
	 echo A file number one > afile1;	\
	 echo A file number two, blah. > afile2; \
	 ln -s afile1 symlink1;			\
	 mkdir there1;				\
	 ${GCP} afile1 afile2 symlink1 there1/;	\
	 ls -lR > ../cp_files_dir.gnu;

	@echo;
	rm -rf cp_tests/{afile{1,2},symlink1,there1};

	@echo;
	cd cp_tests;				\
	 echo A file number one > afile1;	\
	 echo A file number two, blah. > afile2; \
	 ln -s afile1 symlink1;			\
	 mkdir there1;				\
	 ${BCP} afile1 afile2 symlink1 there1/;	\
	 ls -lR > ../cp_files_dir.bb;

	@echo;
	diff -u cp_files_dir.gnu cp_files_dir.bb;

	@echo;
	rm -rf cp_tests/{afile{1,2},symlink1,there1};

	@echo; echo;
	cd cp_tests;				\
	 echo A file number one > afile1;	\
	 echo A file number two, blah. > afile2; \
	 ln -s afile1 symlink1;			\
	 mkdir there1;				\
	 ${GCP} -d afile1 afile2 symlink1 there1/; \
	 ls -lR > ../cp_d_files_dir.gnu;

	@echo;
	rm -rf cp_tests/{afile{1,2},symlink1,there1};

	@echo;
	cd cp_tests;				\
	 echo A file number one > afile1;	\
	 echo A file number two, blah. > afile2; \
	 ln -s afile1 symlink1;			\
	 mkdir there1;				\
	 ${BCP} -d afile1 afile2 symlink1 there1/; \
	 ls -lR > ../cp_d_files_dir.bb;

	@echo;
	diff -u cp_d_files_dir.gnu cp_d_files_dir.bb;

	@echo;
	rm -rf cp_tests/{afile{1,2},symlink1,there1};

	@echo; echo;
	cd cp_tests;				\
	 echo A file number one > afile1;	\
	 echo A file number two, blah. > afile2; \
	 touch --date='Sat Jan 29 21:24:08 PST 2000' afile1; \
	 ln -s afile1 symlink1;			\
	 mkdir there1;				\
	 ${GCP} -p afile1 afile2 symlink1 there1/; \
	 ls -lR > ../cp_p_files_dir.gnu;

	@echo;
	rm -rf cp_tests/{afile{1,2},symlink1,there1};

	@echo;
	cd cp_tests;				\
	 echo A file number one > afile1;	\
	 echo A file number two, blah. > afile2; \
	 touch --date='Sat Jan 29 21:24:08 PST 2000' afile1; \
	 ln -s afile1 symlink1;			\
	 mkdir there1;				\
	 ${BCP} -p afile1 afile2 symlink1 there1/; \
	 ls -lR > ../cp_p_files_dir.bb;

	@echo;
	diff -u cp_p_files_dir.gnu cp_p_files_dir.bb;

	@echo;
	rm -rf cp_tests/{afile{1,2},symlink1,there1};

	@echo; echo;
	cd cp_tests;				\
	 echo A file number one > afile1;	\
	 echo A file number two, blah. > afile2; \
	 touch --date='Sat Jan 29 21:24:08 PST 2000' afile1; \
	 ln -s afile1 symlink1;			\
	 mkdir there1;				\
	 ${GCP} -p -d afile1 afile2 symlink1 there1/; \
	 ls -lR > ../cp_pd_files_dir.gnu;

	@echo;
	rm -rf cp_tests/{afile{1,2},symlink1,there1};

	@echo;
	cd cp_tests;				\
	 echo A file number one > afile1;	\
	 echo A file number two, blah. > afile2; \
	 touch --date='Sat Jan 29 21:24:08 PST 2000' afile1; \
	 ln -s afile1 symlink1;			\
	 mkdir there1;				\
	 ${BCP} -p -d afile1 afile2 symlink1 there1/; \
	 ls -lR > ../cp_pd_files_dir.bb;

	@echo;
	diff -u cp_pd_files_dir.gnu cp_pd_files_dir.bb;

	@echo;
	rm -rf cp_tests/{afile{1,2},symlink1,there1};

	@echo; echo;
	cd cp_tests;				\
	 mkdir dir{a,b};			\
	 echo A file > dira/afile;		\
	 echo A file in dirb > dirb/afileindirb; \
	 ln -s dira/afile dira/alinktoafile;	\
	 mkdir dira/subdir1;			\
	 echo Another file > dira/subdir1/anotherfile; \
	 ls -lR . > ../cp_a_dira_dirb.gnu;	\
	 ${GCP} -a dira dirb;			\
	 ls -lR . >> ../cp_a_dira_dirb.gnu;

	# false;
	@echo;
	rm -rf cp_tests/dir{a,b};

	@echo;
	cd cp_tests;				\
	 mkdir dir{a,b};			\
	 echo A file > dira/afile;		\
	 echo A file in dirb > dirb/afileindirb; \
	 ln -s dira/afile dira/alinktoafile;	\
	 mkdir dira/subdir1;			\
	 echo Another file > dira/subdir1/anotherfile; \
	 ls -lR . > ../cp_a_dira_dirb.bb;	\
	 ${BCP} -a dira dirb;			\
	 ls -lR . >> ../cp_a_dira_dirb.bb;

	@echo;
	diff -u cp_a_dira_dirb.gnu cp_a_dira_dirb.bb;

	# false;
	@echo;
	rm -rf cp_tests/dir{a,b};
