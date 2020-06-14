#include <libstored/directory.h>

#include <string>

static size_t decodeInt(uint8_t const*& p) {
	size_t v = 0;
	while(*p & 0x80) {
		v = (v | (*p & 0x7f)) << 7;
		p++;
	}
	return v | *p++;
}

static void skipOffset(uint8_t const*& p) {
	while(*p++ & 0x80);
}

namespace stored {
namespace impl {

Variant<> find(void* buffer, uint8_t const* directory, char const* name) {
	if(unlikely(!directory || !name)) {
notfound:
		return Variant<>();
	}

	uint8_t const* p = directory;
	while(true) {
		if(*p == 0) {
			// end
			goto notfound;
		} else if(*p >= 0x80) {
			// var
			Type::type type = (Type::type)(*p++ ^ 0x80);
			size_t len = !Type::isFixed(type) ? decodeInt(p) : Type::size(type);
			size_t offset = decodeInt(p);
			if(Type::isFunction(type))
				return Variant<>(type, (unsigned int)offset);
			else
				return Variant<>(type, (char*)buffer + offset, len);
		} else if(!*name) {
			goto notfound;
		} else if(*p <= 0x1f) {
			// skip
			for(uint8_t i = *p++; i > 0; i--, name++) {
				switch(*name) {
				case '\0':
				case '/':
					goto notfound;
				default:;
				}
			}
		} else if(*p == '/') {
			// Skip till next /
			while(*name++ != '/')
				if(!*name)
					goto notfound;
			p++;
		} else {
			// match char
			int c = (int)*name - (int)*p++;
			if(c < 0) {
				// take jmp_l
				p += decodeInt(p) - 1;
			} else {
				skipOffset(p);
				if(c > 0) {
					// take jmp_g
					p += decodeInt(p) - 1;
				} else {
					// equal
					skipOffset(p);
					name++;
				}
			}
		}
	}
}

} // namespace impl

/*!
 * \private
 */
static void list(void* container, void* buffer, uint8_t const* directory, ListCallbackArg* f, void* arg, std::string& name)
{
	if(unlikely(!buffer || !directory))
		return;

	uint8_t const* p = directory;
	size_t erase = 0;

	while(true) {
		if(*p == 0) {
			// end
			break;
		} else if(*p >= 0x80) {
			// var
			Type::type type = (Type::type)(*p++ ^ 0x80);
			size_t len = !Type::isFixed(type) ? decodeInt(p) : Type::size(type);
			size_t offset = decodeInt(p);
			if(Type::isFunction(type))
				f(container, (char const*)name.c_str(), type, (char*)(uintptr_t)offset, 0, arg);
			else
				f(container, (char const*)name.c_str(), type, (char*)buffer + offset, len, arg);
			break;
		} else if(*p <= 0x1f) {
			// skip
			name.append(*p, '?');
			erase += *p;
			p++;
		} else if(*p == '/') {
			// Hierarchy separator.
			name.push_back('/');
			erase++;
			p++;
		} else {
			// next char in name
			char c = (char)*p++;

			// take jmp_l
			size_t jmp = decodeInt(p);
			list(container, buffer, p + jmp - 1, f, arg, name);

			// take jmp_g
			jmp = decodeInt(p);
			list(container, buffer, p + jmp - 1, f, arg, name);

			// resume with this char
			name.push_back(c);
			erase++;
		}
	}

	if(erase)
		name.erase(name.end() - (long)erase, name.end());
}

/*!
 * \ingroup libstored_directory
 */
void list(void* container, void* buffer, uint8_t const* directory, ListCallback* f) {
	list(container, buffer, directory, (ListCallbackArg*)f, nullptr);
}

/*!
 * \ingroup libstored_directory
 */
void list(void* container, void* buffer, uint8_t const* directory, ListCallbackArg* f, void* arg) {
	std::string name;
	list(container, buffer, directory, f, arg, name);
}

} // namespace

