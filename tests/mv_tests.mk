# mv_tests.mk - Set of tests cases for busybox mv
# -------------
# Copyright (C) 2000 Karl M. Hegbloom <karlheg@debian.org> GPL
#

# GNU `mv'
GMV = /bin/mv
# BusyBox `mv'
BMV = $(shell pwd)/mv

all:: mv_tests
clean:: mv_clean

mv_clean:
	rm -rf mv_tests mv_*.{gnu,bb} mv

mv_tests: mv_clean mv
	@echo;
	@echo "No output from diff means busybox mv is functioning properly.";
	@echo;
	@echo "No such file or directory is good; it means the old file got removed.";
	@echo;
	${BMV} || true;

	@echo;
	mkdir mv_tests;

	@echo;
	cd mv_tests;				\
	 echo A file > afile;			\
	 ls -l afile > ../mv_afile_newname.gnu;	\
	 ${GMV} afile newname;			\
	 ls -l newname >> ../mv_afile_newname.gnu;
	-ls -l mv_tests/afile;

	@echo;
	rm -f mv_tests/{afile,newname};

	@echo;
	cd mv_tests;				\
	 echo A file > afile;			\
	 ls -l afile > ../mv_afile_newname.bb;	\
	 ${BMV} afile newname;			\
	 ls -l newname >> ../mv_afile_newname.bb;
	-ls -l mv_tests/afile;

	@echo;
	diff -u mv_afile_newname.gnu mv_afile_newname.bb;

	@echo;
	rm -f mv_tests/{afile,newname};

	@echo; echo ------------------------------;
	cd mv_tests;				\
	 echo A file > afile;			\
	 ln -s afile symlink;			\
	 ls -l afile symlink > ../mv_symlink_newname.gnu; \
	 ${GMV} symlink newname;		\
	 ls -l afile newname >> ../mv_symlink_newname.gnu;
	-ls -l mv_tests/symlink;

	@echo;
	rm -f mv_tests/{afile,newname};

	@echo;
	cd mv_tests;				\
	 echo A file > afile;			\
	 ln -s afile symlink;			\
	 ls -l afile symlink > ../mv_symlink_newname.bb;\
	 ${BMV} symlink newname;		\
	 ls -l afile newname >> ../mv_symlink_newname.bb;
	-ls -l mv_tests/symlink;

	@echo;
	diff -u mv_symlink_newname.gnu mv_symlink_newname.bb;

	@echo;
	rm -rf mv_tests/*;

	@echo; echo ------------------------------;
	cd mv_tests;				\
	 echo A file > afile;			\
	 ln -s afile symlink;			\
	 mkdir newdir;				\
	 ls -lR > ../mv_file_symlink_dir.gnu;	\
	 ${GMV} symlink afile newdir;		\
	 ls -lR >> ../mv_file_symlink_dir.gnu;
	-ls -l mv_tests/{symlink,afile};

	@echo;
	rm -rf mv_tests/*

	@echo; echo ------------------------------;
	cd mv_tests;				\
	 echo A file > afile;			\
	 ln -s afile symlink;			\
	 mkdir newdir;				\
	 ls -lR > ../mv_file_symlink_dir.bb;	\
	 ${BMV} symlink afile newdir;		\
	 ls -lR >> ../mv_file_symlink_dir.bb;
	-ls -l mv_tests/{symlink,afile};

	@echo;
	diff -u mv_file_symlink_dir.gnu mv_file_symlink_dir.bb;

	@echo;
	rm -rf mv_tests/*;

	@echo; echo ------------------------------;
	cd mv_tests;				\
	 mkdir dir{a,b};			\
	 echo A file > dira/afile;		\
	 echo A file in dirb > dirb/afileindirb; \
	 ln -s dira/afile dira/alinktoafile;	\
	 mkdir dira/subdir1;			\
	 echo Another file > dira/subdir1/anotherfile; \
	 ls -lR . > ../mv_dira_dirb.gnu;	\
	 ${GMV} dira dirb;			\
	 ls -lR . >> ../mv_dira_dirb.gnu;

	# false;
	@echo;
	rm -rf mv_tests/dir{a,b};

	@echo;
	cd mv_tests;				\
	 mkdir dir{a,b};			\
	 echo A file > dira/afile;		\
	 echo A file in dirb > dirb/afileindirb; \
	 ln -s dira/afile dira/alinktoafile;	\
	 mkdir dira/subdir1;			\
	 echo Another file > dira/subdir1/anotherfile; \
	 ls -lR . > ../mv_dira_dirb.bb;		\
	 ${BMV} dira dirb;			\
	 ls -lR . >> ../mv_dira_dirb.bb;

	@echo;
	diff -u mv_dira_dirb.gnu mv_dira_dirb.bb;

	# false;
	@echo;
	rm -rf mv_tests/dir{a,b};

	@echo; echo ------------------------------;
	@echo There should be an error message about cannot mv a dir to a subdir of itself.
	cd mv_tests;				\
	 mkdir adir;				\
	 touch -r . a b c adir;			\
	 ls -lR . > ../mv_a_star_adir.gnu;	\
	 ${GMV} * adir;				\
	 ls -lR . >> ../mv_a_star_adir.gnu;

	@echo
	@echo There should be an error message about cannot mv a dir to a subdir of itself.
	cd mv_tests;				\
	 rm -rf a b c adir;			\
	 mkdir adir;				\
	 touch -r . a b c adir;			\
	 ls -lR . > ../mv_a_star_adir.bb;	\
	 ${BMV} * adir;			\
	 ls -lR . >> ../mv_a_star_adir.bb;

	@echo;
	diff -u mv_a_star_adir.gnu mv_a_star_adir.bb;

	@echo;
	rm -rf mv_test/*;
