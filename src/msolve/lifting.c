/* This file is part of msolve.
 *
 * msolve is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * msolve is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with msolve.  If not, see <https://www.gnu.org/licenses/>
 *
 * Authors:
 * Jérémy Berthomieu
 * Christian Eder
 * Mohab Safey El Din */



/* Initialization of sparse fglm matrix for crt */
static inline void crt_mpz_matfglm_initset(crt_mpz_matfglm_t crt_mat,
                                           sp_matfglm_t *mod_mat){
  crt_mat->ncols = mod_mat->ncols;
  crt_mat->nrows = mod_mat->nrows;
  crt_mat->dense_mat = calloc(crt_mat->ncols*crt_mat->nrows,
                              sizeof(mpz_t));
  uint64_t sz = crt_mat->nrows * crt_mat->ncols;
  for(uint64_t i = 0; i < sz; i++){
    mpz_init_set_ui(crt_mat->dense_mat[i], mod_mat->dense_mat[i]);
  }
  long diff = crt_mat->ncols - crt_mat->nrows;
  crt_mat->triv_idx = calloc(diff, sizeof(uint32_t));
  crt_mat->triv_pos = calloc(diff, sizeof(uint32_t));
  crt_mat->dense_idx = calloc(crt_mat->nrows, sizeof(uint32_t));
  crt_mat->dst = calloc(crt_mat->nrows, sizeof(uint32_t));

  for(long i = 0; i < diff; i++){
    crt_mat->triv_idx[i]= mod_mat->triv_idx[i];
    crt_mat->triv_pos[i]= mod_mat->triv_pos[i];
  }
  for(long i = 0; i < crt_mat->nrows; i++){
    crt_mat->dense_idx[i] = mod_mat->dense_idx[i];
    crt_mat->dst[i] = mod_mat->dst[i];
  }
}

/* Initialization of sparse fglm matrix for rational reconstruction */
static inline void mpq_matfglm_initset(mpq_matfglm_t mpq_mat,
                                       sp_matfglm_t *mod_mat){
  mpq_mat->ncols = mod_mat->ncols;
  mpq_mat->nrows = mod_mat->nrows;
  mpq_mat->dense_mat = calloc(2*mpq_mat->ncols*mpq_mat->nrows,
                              sizeof(mpz_t));
  uint64_t sz = mpq_mat->nrows * mpq_mat->ncols;
  uint64_t nc = 2*mpq_mat->ncols;

  for(uint32_t i = 0; i < mpq_mat->nrows; i++){
    uint64_t c = 2*i*mpq_mat->ncols;
    for(uint32_t j = 0; j < nc; j++){
      mpz_init_set_ui(mpq_mat->dense_mat[c+j], 0);
      j++;
      mpz_init_set_ui(mpq_mat->dense_mat[c+j], 1);
    }
  }
  long diff = mpq_mat->ncols - mpq_mat->nrows;
  mpq_mat->triv_idx = calloc(diff, sizeof(uint32_t));
  mpq_mat->triv_pos = calloc(diff, sizeof(uint32_t));
  mpq_mat->dense_idx = calloc(mpq_mat->nrows, sizeof(uint32_t));
  mpq_mat->dst = calloc(mpq_mat->nrows, sizeof(uint32_t));

  for(long i = 0; i < diff; i++){
    mpq_mat->triv_idx[i]= mod_mat->triv_idx[i];
    mpq_mat->triv_pos[i]= mod_mat->triv_pos[i];
  }
  for(long i = 0; i < mpq_mat->nrows; i++){
    mpq_mat->dense_idx[i] = mod_mat->dense_idx[i];
    mpq_mat->dst[i] = mod_mat->dst[i];
  }
}

static inline void crt_lift_dense_rows(mpz_t *rows, uint32_t *mod_rows,
                                       const uint64_t sz,
                                       mpz_t modulus, int32_t prime,
                                       mpz_t prod,
                                       const int nthrds){
  len_t i;

#pragma omp parallel for num_threads(nthrds)    \
  private(i) schedule(static)
  for(i = 0; i < sz; i++){

    mpz_CRT_ui(rows[i], rows[i], modulus,
               mod_rows[i], prime, prod, 1);

  }
}

static inline void crt_lift_mat(crt_mpz_matfglm_t mat, sp_matfglm_t *mod_mat,
                                mpz_t modulus, const int32_t prime,
                                const int nthrds){
  mpz_t prod;
  mpz_init(prod);
  mpz_mul_ui(prod, modulus, prime);
  const uint64_t sz = mat->nrows * mat->ncols;
  crt_lift_dense_rows(mat->dense_mat, mod_mat->dense_mat,
                      sz, modulus, prime, prod, nthrds);
  mpz_clear(prod);
}

static inline int64_t rat_recon_dense_rows(mpq_matfglm_t mpq_mat,
                                            crt_mpz_matfglm_t crt_mat,
                                            mpz_t modulus, rrec_data_t rdata,
                                            mpz_t rnum, mpz_t rden,
                                            long *matrec){
  const uint32_t nrows = crt_mat->nrows;
  const uint32_t ncols = crt_mat->ncols;
  int64_t cnt = 0;
  for(uint32_t i = 0; i < nrows; i++){
    uint64_t c = i*ncols;
    for(uint32_t j = 0; j < ncols; j++){
      int b = 1;
      if(*matrec <= c+j){
        b = ratrecon(rnum, rden, crt_mat->dense_mat[c+j],
                     modulus, rdata);
      }
      if(b == 1){
        cnt++;
      }
      else{
        if(cnt > *matrec+1){
          fprintf(stderr, "<%.2f%%>", (100*(((double)cnt)/ncols))/nrows );
        }
        *matrec = MAX(0, cnt-1);
        return cnt;
      }
    }
  }
  fprintf(stderr, "<100.0%%>\n");
  *matrec = cnt;
  return cnt;
}
