#ifndef __POLY_SYMTABLE_H__
#define __POLY_SYMTABLE_H__

#include <string>

class CSymTable
{
    public:
	CSymTable(int aTok, std::string aStr);
	virtual ~CSymTable();

	double		dVal;
	int		token;
	std::string	strlex;
};

#endif
//martysama0134's dcf42890919f0da1c0e6dbb7f15bc7ec
