#if !defined(CUBE_TEST_PARSE_OBJ_H)
#define CUBE_TEST_PARSE_OBJ_H

struct CTPOParseState
{
    char* buffer;
    uint32_t bytes_read;
};

static bool
ctpo_check(CTPOParseState* parse_state, const char* str)
{
    bool result = true;
    uint32_t idx = 0;
    while (*(str + idx))
    {
        if (*(str + idx) != parse_state->buffer[parse_state->bytes_read + idx++])
        {
            result = false;
            break;
        }
    }
    return result;
}

static void
ctpo_skip_line(CTPOParseState* parse_state)
{
    while (!ctpo_check(parse_state, "\n")) parse_state->bytes_read++;
    parse_state->bytes_read++;
}

static f32
ctpo_parse_f32(CTPOParseState* parse_state)
{
    char* end;
    f32 result = strtof(&parse_state->buffer[parse_state->bytes_read], &end);
    parse_state->bytes_read += (uint32_t)(end - &parse_state->buffer[parse_state->bytes_read]);
    return result;
}

static s32
ctpo_parse_s32(CTPOParseState* parse_state)
{
    char* end;
    f32 result = strtol(&parse_state->buffer[parse_state->bytes_read], &end, 10);
    parse_state->bytes_read += (uint32_t)(end - &parse_state->buffer[parse_state->bytes_read]);
    return result;
}

static void
ctpo_parse(const char* obj_filename,
           V3f* positions, u32* position_count, s32* position_indices,
           V3f* normals,   u32* normal_count,   s32* normal_indices,
           V2f* uvs,       u32* uv_count,       s32* uv_indices,
           u32* index_count)
{
    CTPOParseState parse_state; gj_ZeroStruct(&parse_state);
    
    PlatformFileHandle obj_file_handle = g_platform_api.get_file_handle(obj_filename, PlatformOpenFileModeFlags_Read);
    parse_state.buffer = (char*)g_platform_api.allocate_memory(obj_file_handle.file_size);
    g_platform_api.read_data_from_file_handle(obj_file_handle, 0, obj_file_handle.file_size, parse_state.buffer);
    g_platform_api.close_file_handle(obj_file_handle);
    
    while (parse_state.bytes_read < obj_file_handle.file_size)
    {
        if (ctpo_check(&parse_state, "v "))
        {
            parse_state.bytes_read += 1;
            f32 x = ctpo_parse_f32(&parse_state);
            f32 y = ctpo_parse_f32(&parse_state);
            f32 z = ctpo_parse_f32(&parse_state);
            *positions++ = {x, y, z}; *position_count += 1;
            ctpo_skip_line(&parse_state);
        }
        else if (ctpo_check(&parse_state, "vn"))
        {
            parse_state.bytes_read += 2;
            f32 x = ctpo_parse_f32(&parse_state);
            f32 y = ctpo_parse_f32(&parse_state);
            f32 z = ctpo_parse_f32(&parse_state);
            *normals++ = {x, y, z}; *normal_count += 1;
            ctpo_skip_line(&parse_state);
        }
        else if (ctpo_check(&parse_state, "vt"))
        {
            parse_state.bytes_read += 2;
            f32 x = ctpo_parse_f32(&parse_state);
            f32 y = ctpo_parse_f32(&parse_state);
            *uvs++ = {x, y}; *uv_count += 1;
            ctpo_skip_line(&parse_state);
        }
        else if (ctpo_check(&parse_state, "f"))
        {
            parse_state.bytes_read += 1;
            for (u32 i = 0; i < 3; i++)
            {
                *position_indices++ = ctpo_parse_s32(&parse_state) - 1;
                parse_state.bytes_read++;
                *uv_indices++       = ctpo_parse_s32(&parse_state) - 1;
                parse_state.bytes_read++;
                *normal_indices++   = ctpo_parse_s32(&parse_state) - 1;
                *index_count += 1;
            }
            ctpo_skip_line(&parse_state);
        }
        else
        {
            ctpo_skip_line(&parse_state);
        }
    }
}

#endif
