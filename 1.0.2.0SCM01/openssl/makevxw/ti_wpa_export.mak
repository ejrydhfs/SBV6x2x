WPA_DIR=$(WLANPREFIX)/src/WPA
OPENSSL_DIR=$(WPA_DIR)/openssl
CRYPTO_DIR=$(OPENSSL_DIR)/crypto
SSL_DIR=$(OPENSSL_DIR)/ssl

# dirs in crypto to build
SDIRS=  \
	objects \
	md2 md4 md5 sha hmac ripemd \
	des aes rc2 rc4 idea bf cast \
	bn ec rsa dsa ecdsa dh ecdh dso engine \
	buffer bio stack lhash rand err \
	evp asn1 pem x509 x509v3 conf txt_db pkcs7 pkcs12 comp ocsp ui krb5 \
	store pqueue 
# objects in crypto to build
objects_files = o_names.o obj_dat.o obj_lib.o obj_err.o
md2_files = md2_dgst.o md2_one.o 
md4_files = md4_dgst.o md4_one.o 
md5_files = md5_dgst.o md5_one.o
sha_files = sha_dgst.o sha1dgst.o sha_one.o sha1_one.o sha256.o sha512.o
hmac_files = hmac.o 
ripemd_files = rmd_dgst.o rmd_one.o
des_files = set_key.o  ecb_enc.o  cbc_enc.o ecb3_enc.o cfb64enc.o cfb64ede.o cfb_enc.o  ofb64ede.o enc_read.o enc_writ.o ofb64enc.o ofb_enc.o  str2key.o  pcbc_enc.o qud_cksm.o rand_key.o des_enc.o fcrypt_b.o fcrypt.o xcbc_enc.o rpc_enc.o  cbc_cksm.o ede_cbcm_enc.o des_old.o des_old2.o read2pwd.o
aes_files = aes_misc.o aes_ecb.o aes_cfb.o aes_ofb.o aes_ctr.o aes_core.o aes_cbc.o
rc2_files = rc2_ecb.o rc2_skey.o rc2_cbc.o rc2cfb64.o rc2ofb64.o
rc4_files = rc4_skey.o rc4_enc.o 
idea_files = i_cbc.o i_cfb64.o i_ofb64.o i_ecb.o i_skey.o
bf_files = bf_skey.o bf_ecb.o bf_enc.o bf_cfb64.o bf_ofb64.o 
cast_files = c_skey.o c_ecb.o c_enc.o c_cfb64.o c_ofb64.o
bn_files = bn_add.o bn_div.o bn_exp.o bn_lib.o bn_ctx.o bn_mul.o bn_mod.o bn_print.o bn_rand.o bn_shift.o bn_word.o bn_blind.o bn_kron.o bn_sqrt.o bn_gcd.o bn_prime.o bn_err.o bn_sqr.o bn_asm.o bn_recp.o bn_mont.o bn_mpi.o bn_exp2.o bn_gf2m.o bn_nist.o bn_depr.o bn_const.o 
ec_files = ec_lib.o ecp_smpl.o ecp_mont.o ecp_nist.o ec_cvt.o ec_mult.o ec_err.o ec_curve.o ec_check.o ec_print.o ec_asn1.o ec_key.o ec2_smpl.o ec2_mult.o
rsa_files = rsa_eay.o rsa_gen.o rsa_lib.o rsa_sign.o rsa_saos.o rsa_err.o rsa_pk1.o rsa_ssl.o rsa_none.o rsa_oaep.o rsa_chk.o rsa_null.o rsa_pss.o rsa_x931.o rsa_asn1.o rsa_depr.o 
dsa_files = dsa_gen.o dsa_key.o dsa_lib.o dsa_asn1.o dsa_vrf.o dsa_sign.o dsa_err.o dsa_ossl.o dsa_depr.o
ecdsa_files = ecs_lib.o ecs_asn1.o ecs_ossl.o ecs_sign.o ecs_vrf.o ecs_err.o 
dh_files = dh_asn1.o dh_gen.o dh_key.o dh_lib.o dh_check.o dh_err.o dh_depr.o
ecdh_files = ech_lib.o ech_ossl.o ech_key.o ech_err.o 
dso_files = dso_dl.o dso_dlfcn.o dso_err.o dso_lib.o dso_null.o dso_openssl.o dso_win32.o dso_vms.o
engine_files = eng_err.o eng_lib.o eng_list.o eng_init.o eng_ctrl.o eng_table.o eng_pkey.o eng_fat.o eng_all.o tb_rsa.o tb_dsa.o tb_ecdsa.o tb_dh.o tb_ecdh.o tb_rand.o tb_store.o tb_cipher.o tb_digest.o eng_openssl.o eng_cnf.o eng_dyn.o eng_cryptodev.o eng_padlock.o
buffer_files = buffer.o buf_err.o  
bio_files = bio_lib.o bio_cb.o bio_err.o bss_mem.o bss_null.o bss_fd.o bss_file.o bss_sock.o bss_conn.o bf_null.o bf_buff.o b_print.o b_dump.o b_sock.o bss_acpt.o bf_nbio.o bss_log.o bss_bio.o bss_dgram.o 
stack_files = stack.o 
lhash_files = lhash.o lh_stats.o 
rand_files = md_rand.o randfile.o rand_lib.o rand_err.o rand_egd.o rand_win.o rand_unix.o rand_os2.o rand_nw.o
err_files = err.o err_all.o err_prn.o
evp_files = encode.o digest.o evp_enc.o evp_key.o evp_acnf.o e_des.o e_bf.o e_idea.o e_des3.o e_rc4.o e_aes.o names.o e_xcbc_d.o e_rc2.o e_cast.o e_rc5.o m_null.o m_md2.o m_md4.o m_md5.o m_sha.o m_sha1.o m_dss.o m_dss1.o m_mdc2.o m_ripemd.o m_ecdsa.o p_open.o p_seal.o p_sign.o p_verify.o p_lib.o p_enc.o p_dec.o bio_md.o bio_b64.o bio_enc.o evp_err.o e_null.o c_all.o c_allc.o c_alld.o evp_lib.o bio_ok.o evp_pkey.o evp_pbe.o p5_crpt.o p5_crpt2.o e_old.o
asn1_files = a_object.o a_bitstr.o a_utctm.o a_gentm.o a_time.o a_int.o a_octet.o a_print.o a_type.o a_set.o a_dup.o a_d2i_fp.o a_i2d_fp.o a_enum.o a_utf8.o a_sign.o a_digest.o a_verify.o a_mbstr.o a_strex.o x_algor.o x_val.o x_pubkey.o x_sig.o x_req.o x_attrib.o x_bignum.o x_long.o x_name.o x_x509.o x_x509a.o x_crl.o x_info.o x_spki.o nsseq.o d2i_pu.o d2i_pr.o i2d_pu.o i2d_pr.o t_req.o t_x509.o t_x509a.o t_crl.o t_pkey.o t_spki.o t_bitst.o tasn_new.o tasn_fre.o tasn_enc.o tasn_dec.o tasn_utl.o tasn_typ.o f_int.o f_string.o n_pkey.o f_enum.o a_hdr.o x_pkey.o a_bool.o x_exten.o asn1_gen.o asn1_par.o asn1_lib.o asn1_err.o a_meth.o a_bytes.o a_strnid.o evp_asn1.o asn_pack.o p5_pbe.o p5_pbev2.o p8_pkey.o asn_moid.o
pem_files = pem_sign.o pem_seal.o pem_info.o pem_lib.o pem_all.o pem_err.o pem_x509.o pem_xaux.o pem_oth.o pem_pk8.o pem_pkey.o
x509_files = x509_def.o x509_d2.o x509_r2x.o x509_cmp.o x509_obj.o x509_req.o x509spki.o x509_vfy.o x509_set.o x509cset.o x509rset.o x509_err.o x509name.o x509_v3.o x509_ext.o x509_att.o x509type.o x509_lu.o x_all.o x509_txt.o x509_trs.o by_file.o by_dir.o x509_vpm.o
x509v3_files = v3_bcons.o v3_bitst.o v3_conf.o v3_extku.o v3_ia5.o v3_lib.o v3_prn.o v3_utl.o v3err.o v3_genn.o v3_alt.o v3_skey.o v3_akey.o v3_pku.o v3_int.o v3_enum.o v3_sxnet.o v3_cpols.o v3_crld.o v3_purp.o v3_info.o v3_ocsp.o v3_akeya.o v3_pmaps.o v3_pcons.o v3_ncons.o v3_pcia.o v3_pci.o pcy_cache.o pcy_node.o pcy_data.o pcy_map.o pcy_tree.o pcy_lib.o
conf_files = conf_err.o conf_lib.o conf_api.o conf_def.o conf_mod.o conf_mall.o conf_sap.o
txt_db_files = txt_db.o
pkcs7_files = pk7_asn1.o pk7_lib.o pkcs7err.o pk7_doit.o pk7_smime.o pk7_attr.o pk7_mime.o 
pkcs12_files = p12_add.o p12_asn.o p12_attr.o p12_crpt.o p12_crt.o p12_decr.o p12_init.o p12_key.o p12_kiss.o p12_mutl.o p12_utl.o p12_npas.o pk12err.o p12_p8d.o p12_p8e.o 
comp_files = comp_lib.o comp_err.o c_rle.o c_zlib.o
ocsp_files = ocsp_asn.o ocsp_ext.o ocsp_ht.o ocsp_lib.o ocsp_cl.o ocsp_srv.o ocsp_prn.o ocsp_vfy.o ocsp_err.o
ui_files = ui_err.o ui_lib.o ui_openssl.o ui_util.o ui_compat.o
krb5_files = krb5_asn.o
store_files = str_err.o str_lib.o str_meth.o str_mem.o  
pqueue_files = pqueue.o

CRYPTO_FILES= $(foreach dir,$(SDIRS),$(addprefix $(CRYPTO_DIR)/$(dir)/,$($(dir)_files)))

crypto_files = cryptlib.o mem.o mem_clr.o mem_dbg.o cversion.o ex_data.o tmdiff.o cpt_err.o ebcdic.o uid.o o_time.o o_str.o o_dir.o

CRYPTO_FILES += $(addprefix $(CRYPTO_DIR)/,$(crypto_files))

ssl_files = s2_meth.o  s2_srvr.o  s2_clnt.o  s2_lib.o  s2_enc.o s2_pkt.o s3_meth.o  s3_srvr.o  s3_clnt.o  s3_lib.o  s3_enc.o s3_pkt.o s3_both.o s23_meth.o s23_srvr.o s23_clnt.o s23_lib.o          s23_pkt.o t1_meth.o   t1_srvr.o t1_clnt.o  t1_lib.o  t1_enc.o d1_meth.o   d1_srvr.o d1_clnt.o  d1_lib.o  d1_pkt.o d1_both.o d1_enc.o ssl_lib.o ssl_err2.o ssl_cert.o ssl_sess.o ssl_ciph.o ssl_stat.o ssl_rsa.o ssl_asn1.o ssl_txt.o ssl_algs.o bio_ssl.o ssl_err.o kssl.o

SSL_FILES=$(addprefix $(SSL_DIR)/,$(ssl_files)) 
