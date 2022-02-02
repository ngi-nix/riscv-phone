obj_dep_ = curve25519/curve25519.o curve25519/curve25519-generic.o \
	chacha/chacha.o poly1305/poly1305.o aead/e_chacha20poly1305.o \
	sha/sha256.o sha/sha512.o \
	compat/arc4random.o compat/explicit_bzero.o compat/timingsafe_memcmp.o compat/timingsafe_bcmp.o
obj_dep = $(addprefix crypto/,$(obj_dep_))

subdirs_ = curve25519 chacha poly1305 aead sha aes bf modes compat
subdirs = $(addprefix crypto/,$(subdirs_))
