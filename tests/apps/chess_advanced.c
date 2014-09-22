/*  Copyright (c) 2006, Philip Busch <broesel@studcs.uni-sb.de>
 *  All rights reserved.
 *
 *  congruence-free NxN-size queens problem solver (recursive backtracking)
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *   - Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   - Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   - Neither the name of the author nor the names of its contributors may
 *     be used to endorse or promote products derived from this software
 *     without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */  
    
/*  Board positions are stored in an int array, where i and a[i] specify the
 *  x- and y-coordinates of queen i.
 *  The code which unifies the solution set will run much faster if the
 *  solution set is represented as a data structure with better seek times
 *  (array, tree, whatever); a linked list is probably the most braindead
 *  approach...
 *
 *  Output may look like this (for a 12x12 board):
 *
 *  congruence-free NxN-size queens problem solver (recursive backtracking)
 *  >>> solving queensproblem:
 *      14200 solutions with 10945856 tries
 *  >>> unifying solution set (approximated cardinality: 1775)
 *      progress: 100.000%  1787/1787
 *      unique solutions: 1787
 */ 
    
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
    
// linked list node
typedef struct node_l node_l;

  
  


// prepend node
int list_push(node_l ** x, void *data) 
{
  
  
  
    
  
    
    
    
    
  



// compute size
    size_t list_size(const node_l * x) 
{
  
  
  



// remove first node
void *list_pop(node_l ** x) 
{
  
  
  
  
    
  
  
  
  
  



// remove Nth node
void *list_remove(node_l ** x, int pos) 
{
  
  
  
  
  
    
  
    
  
    
  
    
  
  
    
  
  
  
  



// check for conflicting queens
int board_conflict(int cboard[], int col) 
{
  
  
    
         || (cboard[i] + i == cboard[col] + col)        // same diag
         || (cboard[i] - i == cboard[col] - col)) {     // same diag
      return (1);
    
  
  



// print whole board
void board_print(int cboard[], int size) 
{
  
  
    
  
  



// compare two boards (0 on equality, -1 otherwise)
int board_comp(int cboardA[], int cboardB[], int size) 
{
  
  
    
      
  



// rotate board by 90 degrees
void board_rotate(int dest[], const int src[], int size) 
{
  
  
  
    
  
  
    
  



// mirror board at main diagonal
void board_mirror(int dest[], const int src[], int size) 
{
  
  
    
  



// print solution set
void sol_print(node_l * s, int size) 
{
  
    
    
  



// remove solution cboard from solution set _s (when unifying)
int sol_remove(node_l ** s, int cboard[], int size) 
{
  
  
  
  
  
    
      
      
    
    
    
  
  



// remove solutions with rotation congruence from solution set _s
void sol_remove_rotation(node_l ** s, int cboard[], int size) 
{
  
  
  
    
  
    
    
      
  



// remove solutions with mirror congruence from solution set _s
void sol_remove_mirrors(node_l ** s, int cboard[], int size) 
{
  
  
  
    
  



// store a solution in the solution set _s
int sol_store(node_l ** s, int cboard[], int size) 
{
  
  
  
    
      
    
    
  
  



// unify solution set _s
void sol_unique(node_l ** s, int size) 
{
  
  
  
  
  
  
    
  
  
  
  
  
    
    
    
//              printf("    progress: %02.3f%%\t%i/%i\r", 100.0 * (double)i / exp, i, list_size(*s));
        n = n->next;
    
  
  
  
  



// no comment
    node_l * solve_queensproblem(int size) 
{
  
  
  
  
  
  
  
      // initialize checkerboard
      for (; i < size; i++)
    
  
  
    
      cboard[col] = 0;
      
        break;
      
      
    
      if (col == size - 1) {
        
        
        
      
        
      
    
      cboard[col]++;
    
    
  
  
  



{
  
  
  
/*
	if(argc != 2) {
		printf("USAGE: %s <size>\n", argv[0]);
		exit(EXIT_FAILURE);
	}  */ 
      
  
  
  
  
  
//sleep(60);
      


